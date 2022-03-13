//----------------------------------------------------------------------------------------------------------------------
// File: TrackingService.cpp
// Description:
//----------------------------------------------------------------------------------------------------------------------
#include "TrackingService.hpp"
#include "BryptNode/ServiceProvider.hpp"
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

Awaitable::TrackingService::TrackingService(
    std::shared_ptr<Scheduler::Registrar> const& spRegistrar,
    std::shared_ptr<Node::ServiceProvider> const& spProvider)
    : m_spDelegate()
    , m_trackers()
    , m_ready()
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

std::optional<Awaitable::TrackerKey> Awaitable::TrackingService::StageRequest(
    std::weak_ptr<Peer::Proxy> const& wpRequestee,
    Peer::Action::OnResponse const& onResponse,
    Peer::Action::OnError const& onError,
    Message::Application::Builder& builder)
{
    using namespace Message::Application; 
    assert(Assertions::Threading::IsCoreThread());
    assert(builder.GetDestination());

    constexpr std::string_view CreateMessage = "Creating awaitable tracker for a request to {}. [id={}]";

    if (auto const optTrackerKey = GenerateKey(builder.GetSource()); optTrackerKey) {
        m_logger->debug(CreateMessage, *builder.GetDestination(), *optTrackerKey);
        builder.BindExtension<Extension::Awaitable>(Extension::Awaitable::Request, *optTrackerKey);
        m_trackers.emplace(*optTrackerKey, std::make_unique<RequestTracker>(wpRequestee, onResponse, onError));
        return optTrackerKey;
    }

    return {};
}

//----------------------------------------------------------------------------------------------------------------------

std::optional<Awaitable::TrackerKey> Awaitable::TrackingService::StageDeferred(
    std::weak_ptr<Peer::Proxy> const& wpRequestor,
    std::vector<Node::SharedIdentifier> const& identifiers,
    Message::Application::Parcel const& deferred,
    Message::Application::Builder& builder)
{
    using namespace Message::Application; 
    assert(Assertions::Threading::IsCoreThread());

    constexpr std::string_view CreateMessage = "Creating awaitable tracker to fulfill deferred request from {}. [id={}]";

    if (auto const optExtension = deferred.GetExtension<Extension::Awaitable>(); !optExtension) { return {}; }

    if (auto const optTrackerKey = GenerateKey(builder.GetSource()); optTrackerKey) {
        m_logger->debug(CreateMessage, deferred.GetSource(), *optTrackerKey);
        builder.BindExtension<Extension::Awaitable>(Extension::Awaitable::Request, *optTrackerKey);
        m_trackers.emplace(*optTrackerKey, std::make_unique<AggregateTracker>(wpRequestor, deferred, identifiers));
        return optTrackerKey;
    }

    return {};
}

//----------------------------------------------------------------------------------------------------------------------

bool Awaitable::TrackingService::Process(Message::Application::Parcel&& message)
{
    assert(Assertions::Threading::IsCoreThread());

    constexpr std::string_view MissingWarning = "Received response for awaiting request. [id={}]";
    constexpr std::string_view SuccessMessage = "Received response for awaiting request. [id={}]";
    constexpr std::string_view ExpiredWarning = "Received late response for an expired awaitable from {}. [id={}]";
    constexpr std::string_view UnexpectedWarning = "Received an unexpected response for an awaitable from {}. [id={}]";
    constexpr std::string_view FulfilledMessage = "Deferred request has been fulfilled, waiting to transmit. [id={}]";

    // Try to get the awaitable extension from the supplied message.
    auto const& optExtension = message.GetExtension<Message::Application::Extension::Awaitable>();
    if (!optExtension) { return false; }

    // Try to find the awaiting object in the awaiting container
    auto const key = optExtension->get().GetTracker();
    auto const awaitable = m_trackers.find(key);
    if(awaitable == m_trackers.end()) {
        m_logger->warn(MissingWarning, key);
        return false;
    }

    // Update the response to the waiting message with the new message
    switch (awaitable->second->Update(std::move(message))) {
        case ITracker::UpdateResult::Success: {
             m_logger->debug(SuccessMessage, key);
        } break;
        case ITracker::UpdateResult::Fulfilled: {
            m_logger->debug(FulfilledMessage, key);
            OnTrackerReady(key, std::move(awaitable->second));
        } break;
        case ITracker::UpdateResult::Expired: {
            m_logger->warn(ExpiredWarning, message.GetSource(), key);
            OnTrackerReady(key, std::move(awaitable->second));
        } return false;
        case ITracker::UpdateResult::Unexpected: {
            m_logger->warn(UnexpectedWarning, message.GetSource(), key);
        } return false;
        default: assert(false); return false;
    }

    return true;
}

//----------------------------------------------------------------------------------------------------------------------

bool Awaitable::TrackingService::Process(
    TrackerKey key, Node::Identifier const& identifier, std::vector<std::uint8_t>&& data)
{
    assert(Assertions::Threading::IsCoreThread());

    constexpr std::string_view MissingWarning = "Adding direct response for awaiting request. [id={}]";
    constexpr std::string_view SuccessMessage = "Adding direct response for awaiting request. [id={}]";
    constexpr std::string_view FulfilledMessage = "Request has been fulfilled, ready to process response. [id={}]";

    // Try to find the awaiting object in the awaiting container
    auto const awaitable = m_trackers.find(key);
    if(awaitable == m_trackers.end()) {
        m_logger->warn(MissingWarning, key);
        return false;
    }

    // Update the response to the waiting message with the new message
    switch (awaitable->second->Update(identifier, std::move(data))) {
        case ITracker::UpdateResult::Success: {
             m_logger->debug(SuccessMessage, key);
        } break;
        case ITracker::UpdateResult::Fulfilled: {
            m_logger->debug(FulfilledMessage, key);
            OnTrackerReady(key, std::move(awaitable->second));
        } break;
        default: assert(false); return false;
    }

    return true;
}

//----------------------------------------------------------------------------------------------------------------------

std::size_t Awaitable::TrackingService::Waiting() const
{
    assert(Assertions::Threading::IsCoreThread());
    return m_trackers.size();
}

//----------------------------------------------------------------------------------------------------------------------

std::size_t Awaitable::TrackingService::Ready() const
{
    assert(Assertions::Threading::IsCoreThread());
    return m_ready.size();
}

//----------------------------------------------------------------------------------------------------------------------

void Awaitable::TrackingService::CheckTrackers()
{
    assert(Assertions::Threading::IsCoreThread());

    std::erase_if(m_trackers, [&] (auto&& entry) {
        auto const status = entry.second->GetStatus();
        auto const current = entry.second->CheckStatus();
        bool const ready = current != ITracker::Status::Pending && status != current;
        if (ready) {
            m_ready.emplace_back(std::move(entry.second));
            m_spDelegate->OnTaskAvailable();
        }
        return ready;
    });
}

//----------------------------------------------------------------------------------------------------------------------

std::size_t Awaitable::TrackingService::Execute()
{
    assert(Assertions::Threading::IsCoreThread());

    std::ranges::for_each(m_ready, [this] (auto const& upTracker) {
        [[maybe_unused]] bool const success = upTracker->Fulfill();
    });

    std::size_t executed = m_ready.size();
    m_ready.clear();
    return executed;
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

void Awaitable::TrackingService::OnTrackerReady(TrackerKey key, std::unique_ptr<ITracker>&& upTracker)
{
    assert(Assertions::Threading::IsCoreThread());
    m_ready.emplace_back(std::move(upTracker));
    m_trackers.erase(key);
    m_spDelegate->OnTaskAvailable(); // Notify the scheduler that we have a tasks that can be executed. 
}

//----------------------------------------------------------------------------------------------------------------------

std::size_t Awaitable::TrackingService::KeyHasher::operator()(TrackerKey const& key) const
{
    return boost::hash_range(key.begin(), key.end());
}   

//----------------------------------------------------------------------------------------------------------------------
