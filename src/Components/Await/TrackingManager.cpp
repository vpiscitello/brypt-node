//------------------------------------------------------------------------------------------------
// File: TrackingManager.cpp
// Description:
//------------------------------------------------------------------------------------------------
#include "TrackingManager.hpp"
//------------------------------------------------------------------------------------------------
#include <array>
#include <cassert>
//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
// Description: Creates an await key for a message. Registers the key and the newly created
// ResponseTracker (Single peer) into the AwaitMap for this container
// Returns: the key for the AwaitMap
//------------------------------------------------------------------------------------------------
Await::TrackerKey Await::CTrackingManager::PushRequest(
    std::weak_ptr<CBryptPeer> const& wpRequestor,
    CMessage const& message,
    BryptIdentifier::SharedContainer const& spBryptPeerIdentifier)
{
    Await::TrackerKey const key = KeyGenerator(message.GetPack());
    NodeUtils::printo(
        "Pushing ResponseTracker with key: " + std::to_string(key), NodeUtils::PrintType::Await);
    m_awaiting.emplace(key, CResponseTracker(wpRequestor, message, spBryptPeerIdentifier));
    return key;
}

//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
// Description: Creates an await key for a message. Registers the key and the newly created
// ResponseTracker (Multiple peers) into the AwaitMap for this container
// Returns: the key for the AwaitMap
//------------------------------------------------------------------------------------------------
Await::TrackerKey Await::CTrackingManager::PushRequest(
    std::weak_ptr<CBryptPeer> const& wpRequestor,
    CMessage const& message,
    std::set<BryptIdentifier::SharedContainer> const& identifiers)
{
    Await::TrackerKey const key = KeyGenerator(message.GetPack());
    NodeUtils::printo(
        "Pushing ResponseTracker with key: " + std::to_string(key), NodeUtils::PrintType::Await);
    m_awaiting.emplace(key, CResponseTracker(wpRequestor, message, identifiers));
    return key;
}

//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
// Description: Pushes a response message onto the await object, finds the key from the message
// await ID
// Returns: boolean indicating success or not
//------------------------------------------------------------------------------------------------
bool Await::CTrackingManager::PushResponse(CMessage const& message)
{
    // Try to get an await object ID from the message
    auto const& optKey = message.GetAwaitingKey();
    if (!optKey) {
        return false;
    }

    // Try to find the awaiting object in the awaiting container
    auto const itr = m_awaiting.find(*optKey);
    if(itr == m_awaiting.end()) {
        return false;
    }
    auto& [key, tracker] = *itr;
    assert(key == *optKey);

    // Update the response to the waiting message with th new message
    NodeUtils::printo(
        "Pushing response to ResponseTracker " + std::to_string(key), NodeUtils::PrintType::Await);

    auto const status = tracker.UpdateResponse(message);
    if (status == ResponseStatus::Fulfilled) {
        NodeUtils::printo(
            "ResponseTracker has been fulfilled, Waiting to transmit", NodeUtils::PrintType::Await);
    }

    return true;
}

//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
// Description: 
//------------------------------------------------------------------------------------------------
void Await::CTrackingManager::ProcessFulfilledRequests()
{
    for (auto itr = m_awaiting.begin(); itr != m_awaiting.end();) {
        auto& [key, tracker] = *itr;
        if (tracker.CheckResponseStatus() == ResponseStatus::Fulfilled) {
            NodeUtils::printo(
                "Sending fulfilled response for:  " + std::to_string(key),
                NodeUtils::PrintType::Await);
            [[maybe_unused]] bool const bSuccess = tracker.SendFulfilledResponse();
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
Await::TrackerKey Await::CTrackingManager::KeyGenerator(std::string_view pack) const
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
