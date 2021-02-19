//------------------------------------------------------------------------------------------------
// File: TrackingManager.cpp
// Description:
//------------------------------------------------------------------------------------------------
#include "TrackingManager.hpp"
#include "Utilities/LogUtils.hpp"
//------------------------------------------------------------------------------------------------
#include <openssl/md5.h>
//------------------------------------------------------------------------------------------------
#include <array>
#include <cassert>
//------------------------------------------------------------------------------------------------

Await::TrackingManager::TrackingManager()
    : m_awaiting()
    , m_spLogger(spdlog::get(LogUtils::Name::Core.data()))
{
    assert(m_spLogger);
}

//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
// Description: Creates an await key for a message. Registers the key and the newly created
// ResponseTracker (Single peer) into the AwaitMap for this container
// Returns: the key for the AwaitMap
//------------------------------------------------------------------------------------------------
Await::TrackerKey Await::TrackingManager::PushRequest(
    std::weak_ptr<BryptPeer> const& wpRequestor,
    ApplicationMessage const& message,
    BryptIdentifier::SharedContainer const& spBryptPeerIdentifier)
{
    Await::TrackerKey const key = KeyGenerator(message.GetPack());
    m_spLogger->debug(
        "Spawning tracker to fulfill awaiting request from {}. [request={:x}]",
        key, message.GetSourceIdentifier());
    m_awaiting.emplace(key, ResponseTracker(wpRequestor, message, spBryptPeerIdentifier));
    return key;
}

//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
// Description: Creates an await key for a message. Registers the key and the newly created
// ResponseTracker (Multiple peers) into the AwaitMap for this container
// Returns: the key for the AwaitMap
//------------------------------------------------------------------------------------------------
Await::TrackerKey Await::TrackingManager::PushRequest(
    std::weak_ptr<BryptPeer> const& wpRequestor,
    ApplicationMessage const& message,
    std::set<BryptIdentifier::SharedContainer> const& identifiers)
{
    Await::TrackerKey const key = KeyGenerator(message.GetPack());
    m_spLogger->debug(
        "Spawning tracker to fulfill awaiting request from {}. [request={:x}]",
        message.GetSourceIdentifier(), key);
    m_awaiting.emplace(key, ResponseTracker(wpRequestor, message, identifiers));
    return key;
}

//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
// Description: Pushes a response message onto the await object, finds the key from the message
// await ID
// Returns: boolean indicating success or not
//------------------------------------------------------------------------------------------------
bool Await::TrackingManager::PushResponse(ApplicationMessage const& message)
{
    // Try to get an await object ID from the message
    auto const& optKey = message.GetAwaitTrackerKey();
    if (!optKey) { return false; }

    // Try to find the awaiting object in the awaiting container
    auto const itr = m_awaiting.find(*optKey);
    if(itr == m_awaiting.end()) {
        m_spLogger->warn(
            "Unable to find an awaiting request for id={:x}."
            "The request may have exist or has expired.", *optKey);
        return false;
    }

    auto& [key, tracker] = *itr;
    assert(key == *optKey);

    // Update the response to the waiting message with th new message
    switch (tracker.UpdateResponse(message)) {
        // Response tracter update success handling. 
        // The tracker will notify us if the response update was successful or if on success the
        // awaiting request became fulfilled. 
        case UpdateStatus::Success: {
            m_spLogger->debug("Received response for awaiting request. [request={:x}].", key);
        } break;
        case UpdateStatus::Fulfilled: {
            m_spLogger->debug(
                "Await request has been fulfilled, waiting to transmit. [request={:x}]", key);
        } break;
        // Response tracker update error handling.
        // The tracker will us us if the response was the received response was received after
        // allowable period. This may only occur while the response is waiting for transmission
        // and we able to determine the associated request for the message. 
        case UpdateStatus::Expired: {
            m_spLogger->warn(
                "Expired await request for {} received a late response from {}. [request={:x}]",
                tracker.GetSource(),
                message.GetSourceIdentifier(), key);
        } return false;
        case UpdateStatus::Unexpected: {
            m_spLogger->warn(
                "Await request for {} received an unexpected response from {}. [request={:x}]",
                tracker.GetSource(), 
                message.GetSourceIdentifier(), key);
        } return false;
        default: assert(false); return false;
    }

    return true;
}

//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
// Description: 
//------------------------------------------------------------------------------------------------
void Await::TrackingManager::ProcessFulfilledRequests()
{
    for (auto itr = m_awaiting.begin(); itr != m_awaiting.end();) {
        auto& [key, tracker] = *itr;
        if (tracker.CheckResponseStatus() == ResponseStatus::Fulfilled) {
            bool const success = tracker.SendFulfilledResponse();
            if (success) {
                m_spLogger->debug(  
                    "Await request has been transmitted to {}. [request={:x}]",
                    tracker.GetSource(), key);
            } else {
                m_spLogger->warn(  
                    "Unable to fulfill request from {}. [request={:x}]",
                    key, tracker.GetSource());
            }
            itr = m_awaiting.erase(itr);
        } else {
            ++itr;
        }
    }
}

//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
// Description:
//------------------------------------------------------------------------------------------------
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

//------------------------------------------------------------------------------------------------
