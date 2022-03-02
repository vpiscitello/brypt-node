//----------------------------------------------------------------------------------------------------------------------
// File: TrackingService.cpp
// Description:
//----------------------------------------------------------------------------------------------------------------------
#include "TrackingService.hpp"
#include "Components/MessageControl/AuthorizedProcessor.hpp"
#include "Components/Scheduler/Delegate.hpp"
#include "Components/Scheduler/Registrar.hpp"
#include "Utilities/Logger.hpp"
//----------------------------------------------------------------------------------------------------------------------
#include <boost/container_hash/hash.hpp>
#include <openssl/md5.h>
#include <openssl/rand.h>
#include <spdlog/fmt/bundled/ranges.h>
//----------------------------------------------------------------------------------------------------------------------
#include <array>
#include <cassert>
//----------------------------------------------------------------------------------------------------------------------

Awaitable::TrackingService::TrackingService(std::shared_ptr<Scheduler::Registrar> const& spRegistrar)
    : m_spDelegate()
    , m_trackers()
    , m_logger(spdlog::get(Logger::Name::Core.data()))
{
    assert(Assertions::Threading::IsCoreThread());
    assert(m_logger);

    m_spDelegate = spRegistrar->Register<TrackingService>([this] () -> std::size_t {
        return Execute();  // Dispatch any fulfilled awaiting messages since this last cycle. 
    }); 

    assert(m_spDelegate);
	m_spDelegate->Depends<AuthorizedProcessor>(); // Allows us to send messages fulfilled during the currect cycle. 
}

//----------------------------------------------------------------------------------------------------------------------

Awaitable::TrackingService::~TrackingService()
{
    m_spDelegate->Delist();
}

//----------------------------------------------------------------------------------------------------------------------

std::optional<Awaitable::TrackerKey> Awaitable::TrackingService::StageDeferred(
    std::weak_ptr<Peer::Proxy> const& wpRequestor,
    std::vector<Node::SharedIdentifier> const& identifiers,
    Message::Application::Parcel const& deferred,
    Message::Application::Builder& builder)
{
    assert(Assertions::Threading::IsCoreThread());

    if (auto const optExtension = deferred.GetExtension<Message::Application::Extension::Awaitable>(); !optExtension) {
        return {};
    }

    if (auto const optTrackerKey = GenerateKey(builder.GetSource()); optTrackerKey) {
        builder.BindExtension<Message::Application::Extension::Awaitable>(
            Message::Application::Extension::Awaitable::Request, *optTrackerKey);
        m_logger->debug(
            "Creating awaitable tracker to fulfill defered request from {}. [id={}]",
            deferred.GetSource(), *optTrackerKey);
        m_trackers.emplace(*optTrackerKey, std::make_unique<AggregateTracker>(wpRequestor, deferred, identifiers));
        return optTrackerKey;
    }

    return {};
}

//----------------------------------------------------------------------------------------------------------------------

bool Awaitable::TrackingService::Process(Message::Application::Parcel const& message)
{
    assert(Assertions::Threading::IsCoreThread());

    // Try to get the awaitable extension from the supplied message.
    auto const& optExtension = message.GetExtension<Message::Application::Extension::Awaitable>();
    if (!optExtension) { return false; }

    // Try to find the awaiting object in the awaiting container
    auto const itr = m_trackers.find(optExtension->get().GetTracker());
    if(itr == m_trackers.end()) {
        m_logger->warn(
            "Unable to find an awaiting request for id={}. The request may have exist or has expired.",
            optExtension->get().GetTracker());
        return false;
    }

    // Update the response to the waiting message with th new message
    auto& [key, upTracker] = *itr;
    assert(upTracker);

    switch (upTracker->Update(message)) {
        // The tracker will notify us if the response update was successful or if on success the awaiting request
        //  became fulfilled. 
        case ITracker::UpdateResult::Success: {
            m_logger->debug("Received response for awaiting request. [id={}].", key);
        } break;
        case ITracker::UpdateResult::Fulfilled: {
            m_logger->debug("Await request has been fulfilled, waiting to transmit. [id={}]", key);
            m_spDelegate->OnTaskAvailable(); // Notify the scheduler that we have a tasks that can be executed. 
        } break;
        // The tracker will us us if the response was the received response was received after allowable period. This may
        //  only occur while the response is waiting for transmission and we able to determine the associated request 
        // for the message. 
        case ITracker::UpdateResult::Expired: {
            m_logger->warn(
                "Received late response for an expired awaitable from {}. [id={}]", message.GetSource(), key);
        } return false;
        case ITracker::UpdateResult::Unexpected: {
            m_logger->warn(
                "Received an unexpected response for an awaitable from {}. [id={}]", message.GetSource(), key);
        } return false;
        default: assert(false); return false;
    }

    return true;
}

//----------------------------------------------------------------------------------------------------------------------

std::size_t Awaitable::TrackingService::Execute()
{
    assert(Assertions::Threading::IsCoreThread());

    std::size_t const count = m_trackers.size();
    for (auto itr = m_trackers.begin(); itr != m_trackers.end();) {
        auto& [key, upTracker] = *itr;
        assert(upTracker);

        if (upTracker->CheckStatus() == ITracker::Status::Fulfilled) {
            bool const success = upTracker->Fulfill();
            if (success) {
                m_logger->debug("Awaitable has been transmitted. [id={}]",  key);
            } else {
                m_logger->warn("Unable to fulfill awaitable. [id={}]", key);
            }
            itr = m_trackers.erase(itr);
        } else {
            ++itr;
        }
    }

    return count - m_trackers.size(); // Return the total number of messages fulfilled. 
}

//----------------------------------------------------------------------------------------------------------------------

std::optional<Awaitable::TrackerKey> Awaitable::TrackingService::GenerateKey(Node::Identifier const& identifier) const
{
    std::array<std::uint8_t, MD5_DIGEST_LENGTH> digest;

    MD5_CTX ctx;
    if (!MD5_Init(&ctx)) { return {}; }

    Node::Internal::Identifier const internal = identifier;
    if (!MD5_Update(&ctx, &internal, sizeof(internal))) { return {}; }

    auto const timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    if (!MD5_Update(&ctx, &timestamp, sizeof(timestamp))) { return {}; }

    std::array<std::uint8_t, 8> salt;
    if (!RAND_bytes(salt.data(), salt.size())) { return {}; }
    if (!MD5_Update(&ctx, salt.data(), salt.size())) { return {}; }

    if (!MD5_Final(digest.data(), &ctx)) { return {}; }

    return digest;
}

//----------------------------------------------------------------------------------------------------------------------

std::size_t Awaitable::TrackingService::KeyHasher::operator()(TrackerKey const& key) const {
    return boost::hash_range(key.begin(), key.end());
}   

//----------------------------------------------------------------------------------------------------------------------
