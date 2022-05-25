//----------------------------------------------------------------------------------------------------------------------
// File: TrackingService.cpp
// Description:
//----------------------------------------------------------------------------------------------------------------------
#include "TrackingService.hpp"
#include "BryptNode/ServiceProvider.hpp"
#include "Components/MessageControl/AuthorizedProcessor.hpp"
#include "Components/Scheduler/Delegate.hpp"
#include "Components/Scheduler/Registrar.hpp"
#include "Components/Scheduler/TaskService.hpp"
#include "Utilities/Logger.hpp"
//----------------------------------------------------------------------------------------------------------------------
#include <boost/container_hash/hash.hpp>
#include <openssl/err.h>
#include <openssl/md5.h>
#include <openssl/rand.h>
#include <spdlog/fmt/bundled/ranges.h>
//----------------------------------------------------------------------------------------------------------------------
#include <array>
#include <cassert>
#include <chrono>
//----------------------------------------------------------------------------------------------------------------------

Awaitable::TrackingService::TrackingService(std::shared_ptr<Scheduler::Registrar> const& spRegistrar)
    : m_spDelegate()
    , m_mutex()
    , m_trackers()
    , m_ready()
    , m_logger(spdlog::get(Logger::Name::Core.data()))
{
    assert(Assertions::Threading::IsCoreThread());
    assert(m_logger);

    m_spDelegate = spRegistrar->Register<TrackingService>([this] (Scheduler::Frame const&) -> std::size_t {
        return Execute();  // Dispatch any fulfilled awaiting messages since this last cycle. 
    }); 

    m_spDelegate->Schedule([this] { CheckTrackers(); }, CheckInterval);

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
    using namespace Message; 
    assert(builder.GetDestination());

    constexpr std::string_view CreateMessage = "Creating awaitable tracker for a request to {}. [id={}]";

    if (auto const optTrackerKey = GenerateKey(builder.GetSource()); optTrackerKey) {
        std::scoped_lock lock{ m_mutex };
        m_logger->debug(CreateMessage, *builder.GetDestination(), *optTrackerKey);
        builder.BindExtension<Extension::Awaitable>(Extension::Awaitable::Request, *optTrackerKey);
        m_trackers.emplace(
            *optTrackerKey, std::make_unique<RequestTracker>(*optTrackerKey, wpRequestee, onResponse, onError));
        return optTrackerKey;
    }

    return {};
}

//----------------------------------------------------------------------------------------------------------------------

std::optional<Awaitable::TrackingService::Correlatable> Awaitable::TrackingService::StageRequest(
    Node::Identifier const& self,
    std::size_t expected,
    Peer::Action::OnResponse const& onResponse,
    Peer::Action::OnError const& onError)
{
    using namespace Message; 
    constexpr std::string_view CreateMessage = "Staging awaitable tracker for a request to the cluster. [id={}]";

    if (auto const optTrackerKey = GenerateKey(self); optTrackerKey) {
        std::scoped_lock lock{ m_mutex };
        m_logger->debug(CreateMessage, *optTrackerKey);
        auto const spTracker = std::make_shared<RequestTracker>(*optTrackerKey, expected, onResponse, onError);
        m_trackers.emplace(*optTrackerKey, spTracker);
        return std::make_pair(*optTrackerKey, [spTracker] (auto const& spIdentifier) {
            return spTracker->Correlate(spIdentifier);
        });
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
    using namespace Message; 

    constexpr std::string_view CreateMessage = "Creating awaitable tracker to fulfill deferred request from {}. [id={}]";

    if (auto const optExtension = deferred.GetExtension<Extension::Awaitable>(); !optExtension) { return {}; }

    if (auto const optTrackerKey = GenerateKey(builder.GetSource()); optTrackerKey) {
        std::scoped_lock lock{ m_mutex };
        m_logger->debug(CreateMessage, deferred.GetSource(), *optTrackerKey);
        builder.BindExtension<Extension::Awaitable>(Extension::Awaitable::Request, *optTrackerKey);
        m_trackers.emplace(
            *optTrackerKey, std::make_shared<DeferredTracker>(*optTrackerKey, wpRequestor, deferred, identifiers));
        return optTrackerKey;
    }

    return {};
}

//----------------------------------------------------------------------------------------------------------------------

void Awaitable::TrackingService::Cancel(Awaitable::TrackerKey const& key)
{
    constexpr std::string_view CancelMessage = "Canceling awaitable... [id={}]";

    std::scoped_lock lock{ m_mutex };
    m_logger->debug(CancelMessage, key);
    m_trackers.erase(key);
}

//----------------------------------------------------------------------------------------------------------------------

bool Awaitable::TrackingService::Process(Message::Application::Parcel&& message)
{
    constexpr std::string_view SuccessMessage = "Received a response for an awaitable. [id={}]";
    constexpr std::string_view MissingWarning = "Ignoring message for an unknown awaitable from {}. [id={}]";
    constexpr std::string_view ExpiredWarning = "Ignoring late response for an expired awaitable from {}. [id={}]";
    constexpr std::string_view UnexpectedWarning = "Ignoring an unexpected response for an awaitable from {}. [id={}]";
    constexpr std::string_view FulfilledMessage = "Awaitable has been fulfilled, waiting for processing. [id={}]";

    // Try to get the awaitable extension from the supplied message.
    auto const& optExtension = message.GetExtension<Message::Extension::Awaitable>();
    if (!optExtension) { return false; }

    auto const key = optExtension->get().GetTracker(); 

    // Try to find the awaiting object in the awaiting container
    std::scoped_lock lock{ m_mutex };
    auto const awaitable = m_trackers.find(key);
    if(awaitable == m_trackers.end()) {
        m_logger->warn(MissingWarning, message.GetSource(), key);
        return false;
    }

    auto const OnTrackerReady = [this] (TrackerKey key, std::shared_ptr<ITracker>&& spTracker) {
        m_ready.emplace_back(std::move(spTracker));
        m_trackers.erase(key);
        m_spDelegate->OnTaskAvailable(); // Notify the scheduler that we have a tasks that can be executed. 
    };

    auto const OnTrackerPartial = [this] (std::shared_ptr<ITracker> const& spTracker) {
        m_ready.emplace_back(spTracker);
        m_spDelegate->OnTaskAvailable(); // Notify the scheduler that we have a tasks that can be executed. 
    };

    // Update the response to the waiting message with the new message
    switch (awaitable->second->Update(std::move(message))) {
        case ITracker::UpdateResult::Success: {
             m_logger->debug(SuccessMessage, key);
        } break;
        case ITracker::UpdateResult::Partial: {
            m_logger->debug(SuccessMessage, key);
            OnTrackerPartial(awaitable->second);
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
    TrackerKey key, Node::Identifier const& identifier, Message::Payload&& data)
{
    constexpr std::string_view MissingWarning = "Adding direct response for awaiting request. [id={}]";
    constexpr std::string_view SuccessMessage = "Adding direct response for awaiting request. [id={}]";
    constexpr std::string_view FulfilledMessage = "Request has been fulfilled, ready to process response. [id={}]";

    // Try to find the awaiting object in the awaiting container
    std::scoped_lock lock{ m_mutex };
    auto const awaitable = m_trackers.find(key);
    if(awaitable == m_trackers.end()) {
        m_logger->warn(MissingWarning, key);
        return false;
    }

    auto const OnTrackerReady = [this] (TrackerKey key, std::shared_ptr<ITracker>&& spTracker) {
        m_ready.emplace_back(std::move(spTracker));
        m_trackers.erase(key);
        m_spDelegate->OnTaskAvailable(); // Notify the scheduler that we have a tasks that can be executed. 
    };

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
    std::scoped_lock lock{ m_mutex };
    return m_trackers.size();
}

//----------------------------------------------------------------------------------------------------------------------

std::size_t Awaitable::TrackingService::Ready() const
{
    std::scoped_lock lock{ m_mutex };
    return m_ready.size();
}

//----------------------------------------------------------------------------------------------------------------------

std::size_t Awaitable::TrackingService::Execute()
{
    std::scoped_lock lock{ m_mutex };

    std::ranges::for_each(m_ready, [this] (auto const& spTracker) {
        [[maybe_unused]] bool const success = spTracker->Fulfill();
    });

    std::size_t executed = m_ready.size();
    m_ready.clear();
    return executed;
}

//----------------------------------------------------------------------------------------------------------------------

void Awaitable::TrackingService::CheckTrackers()
{
    std::scoped_lock lock{ m_mutex };
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

std::optional<Awaitable::TrackerKey> Awaitable::TrackingService::GenerateKey(Node::Identifier const& identifier) const
{
    std::unique_ptr<EVP_MD_CTX, decltype(&EVP_MD_CTX_free)> upDigestContext(EVP_MD_CTX_new(), &EVP_MD_CTX_free);
    std::array<std::uint8_t, MD5_DIGEST_LENGTH> digest{ 0 };
	
    if (ERR_get_error() != 0 || upDigestContext == nullptr) { return {}; }

    if(!EVP_DigestInit_ex(upDigestContext.get(), EVP_md5(), nullptr)) { return { }; }

    Node::Internal::Identifier const internal = identifier;
    if (!EVP_DigestUpdate(upDigestContext.get(), &internal, sizeof(internal))) { return { }; }

    auto const timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    if (!EVP_DigestUpdate(upDigestContext.get(), &timestamp, sizeof(timestamp))) { return { }; }

    constexpr std::int32_t SaltSize = 8;
    std::array<std::uint8_t, SaltSize> salt;
    if (!RAND_bytes(salt.data(), SaltSize)) { return { }; }
    if (!EVP_DigestUpdate(upDigestContext.get(), salt.data(), salt.size())) { return { }; }

    std::uint32_t length = MD5_DIGEST_LENGTH;
    if (!EVP_DigestFinal_ex(upDigestContext.get(), digest.data(), &length)) { return { }; }
    if(length != digest.size()) { return { }; }

    return digest;
}

//----------------------------------------------------------------------------------------------------------------------

std::size_t Awaitable::TrackingService::KeyHasher::operator()(TrackerKey const& key) const
{
    return boost::hash_range(key.begin(), key.end());
}   

//----------------------------------------------------------------------------------------------------------------------
