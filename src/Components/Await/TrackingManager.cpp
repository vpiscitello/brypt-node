//----------------------------------------------------------------------------------------------------------------------
// File: TrackingManager.cpp
// Description:
//----------------------------------------------------------------------------------------------------------------------
#include "TrackingManager.hpp"
#include "Components/MessageControl/AuthorizedProcessor.hpp"
#include "Components/Scheduler/Delegate.hpp"
#include "Components/Scheduler/Service.hpp"
#include "Utilities/Logger.hpp"
//----------------------------------------------------------------------------------------------------------------------
#include <openssl/md5.h>
//----------------------------------------------------------------------------------------------------------------------
#include <array>
#include <cassert>
//----------------------------------------------------------------------------------------------------------------------

class AuthorizedProcessor;

//----------------------------------------------------------------------------------------------------------------------

Await::TrackingManager::TrackingManager(std::shared_ptr<Scheduler::Service> const& spScheduler)
    : m_spDelegate()
    , m_awaiting()
    , m_logger(spdlog::get(Logger::Name::Core.data()))
{
    assert(Assertions::Threading::IsCoreThread());
    assert(m_logger);

    m_spDelegate = spScheduler->Register<TrackingManager>([this] () -> std::size_t {
        return ProcessFulfilledRequests();  // Dispatch any fulfilled awaiting messages since this last cycle. 
    }); 

    assert(m_spDelegate);
	m_spDelegate->Depends<AuthorizedProcessor>(); // Allows us to send messages fulfilled during the currect cycle. 
}

//----------------------------------------------------------------------------------------------------------------------

//----------------------------------------------------------------------------------------------------------------------
// Description: Creates an await key for a message. Registers the key and the newly created
// ResponseTracker (Single peer) into the AwaitMap for this container
// Returns: the key for the AwaitMap
//----------------------------------------------------------------------------------------------------------------------
Await::TrackerKey Await::TrackingManager::PushRequest(
    std::weak_ptr<Peer::Proxy> const& wpRequestor,
    ApplicationMessage const& message,
    Node::SharedIdentifier const& spIdentifier)
{
    assert(Assertions::Threading::IsCoreThread());
    Await::TrackerKey const key = KeyGenerator(message.GetPack());
    m_logger->debug(
        "Spawning tracker to fulfill awaiting request from {}. [request={:x}]", key, message.GetSourceIdentifier());
    m_awaiting.emplace(key, ResponseTracker(wpRequestor, message, spIdentifier));
    return key;
}

//----------------------------------------------------------------------------------------------------------------------

//----------------------------------------------------------------------------------------------------------------------
// Description: Creates an await key for a message. Registers the key and the newly created
// ResponseTracker (Multiple peers) into the AwaitMap for this container
// Returns: the key for the AwaitMap
//----------------------------------------------------------------------------------------------------------------------
Await::TrackerKey Await::TrackingManager::PushRequest(
    std::weak_ptr<Peer::Proxy> const& wpRequestor,
    ApplicationMessage const& message,
    std::set<Node::SharedIdentifier> const& identifiers)
{
    assert(Assertions::Threading::IsCoreThread());
    Await::TrackerKey const key = KeyGenerator(message.GetPack());
    m_logger->debug(
        "Spawning tracker to fulfill awaiting request from {}. [request={:x}]", message.GetSourceIdentifier(), key);
    m_awaiting.emplace(key, ResponseTracker(wpRequestor, message, identifiers));
    return key;
}

//----------------------------------------------------------------------------------------------------------------------

//----------------------------------------------------------------------------------------------------------------------
// Description: Pushes a response message onto the await object, finds the key from the message await identifier.
// Returns: boolean indicating success or not
//----------------------------------------------------------------------------------------------------------------------
bool Await::TrackingManager::PushResponse(ApplicationMessage const& message)
{
    assert(Assertions::Threading::IsCoreThread());
    // Try to get an await object ID from the message
    auto const& optKey = message.GetAwaitTrackerKey();
    if (!optKey) { return false; }

    // Try to find the awaiting object in the awaiting container
    auto const itr = m_awaiting.find(*optKey);
    if(itr == m_awaiting.end()) {
        m_logger->warn(
            "Unable to find an awaiting request for id={:x}. The request may have exist or has expired.", *optKey);
        return false;
    }

    auto& [key, tracker] = *itr;
    assert(key == *optKey);

    // Update the response to the waiting message with th new message
    switch (tracker.UpdateResponse(message)) {
        // The tracker will notify us if the response update was successful or if on success the awaiting request
        //  became fulfilled. 
        case UpdateStatus::Success: {
            m_logger->debug("Received response for awaiting request. [request={:x}].", key);
        } break;
        case UpdateStatus::Fulfilled: {
            m_logger->debug("Await request has been fulfilled, waiting to transmit. [request={:x}]", key);
            m_spDelegate->OnTaskAvailable(); // Notify the scheduler that we have a tasks that can be executed. 
        } break;
        // The tracker will us us if the response was the received response was received after allowable period. This may
        //  only occur while the response is waiting for transmission and we able to determine the associated request 
        // for the message. 
        case UpdateStatus::Expired: {
            m_logger->warn(
                "Expired await request for {} received a late response from {}. [request={:x}]",
                tracker.GetSource(), message.GetSourceIdentifier(), key);
        } return false;
        case UpdateStatus::Unexpected: {
            m_logger->warn(
                "Await request for {} received an unexpected response from {}. [request={:x}]",
                tracker.GetSource(), message.GetSourceIdentifier(), key);
        } return false;
        default: assert(false); return false;
    }

    return true;
}

//----------------------------------------------------------------------------------------------------------------------

std::size_t  Await::TrackingManager::ProcessFulfilledRequests()
{
    assert(Assertions::Threading::IsCoreThread());

    std::size_t const count = m_awaiting.size();
    for (auto itr = m_awaiting.begin(); itr != m_awaiting.end();) {
        auto& [key, tracker] = *itr;
        if (tracker.CheckResponseStatus() == ResponseStatus::Fulfilled) {
            bool const success = tracker.SendFulfilledResponse();
            if (success) {
                m_logger->debug("Await request has been transmitted to {}. [request={:x}]", tracker.GetSource(), key);
            } else {
                m_logger->warn("Unable to fulfill request from {}. [request={:x}]", key, tracker.GetSource());
            }
            itr = m_awaiting.erase(itr);
        } else {
            ++itr;
        }
    }

    return count - m_awaiting.size(); // Return the total number of messages fulfilled. 
}

//----------------------------------------------------------------------------------------------------------------------

Await::TrackerKey Await::TrackingManager::KeyGenerator(std::string_view pack) const
{
    std::array<std::uint8_t, MD5_DIGEST_LENGTH> digest;

    MD5_CTX ctx;
    MD5_Init(&ctx);
    MD5_Update(&ctx, pack.data(), pack.size());
    MD5_Final(digest.data(), &ctx);

    Await::TrackerKey key = 0;
    std::memcpy(&key, digest.data(), sizeof(key)); // Truncate the 128 bit hash to 32 bits
    return key;
}

//----------------------------------------------------------------------------------------------------------------------
