//------------------------------------------------------------------------------------------------
// File: TrackingManager.hpp
// Description:
//------------------------------------------------------------------------------------------------
#pragma once
//------------------------------------------------------------------------------------------------
#include "AwaitDefinitions.hpp"
#include "ResponseTracker.hpp"
#include "../../BryptIdentifier/BryptIdentifier.hpp"
//------------------------------------------------------------------------------------------------
#include <openssl/md5.h>
//------------------------------------------------------------------------------------------------
#include <set>
#include <string_view>
#include <unordered_map>
#include <vector>
//------------------------------------------------------------------------------------------------

class CApplicationMessage;
class CBryptPeer;

//------------------------------------------------------------------------------------------------
namespace Await {
//------------------------------------------------------------------------------------------------

class CTrackingManager;

//------------------------------------------------------------------------------------------------
} // Await namespace
//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
// Description:
//------------------------------------------------------------------------------------------------
class Await::CTrackingManager
{
public:
    TrackerKey PushRequest(
        std::weak_ptr<CBryptPeer> const& wpRequestor,
        CApplicationMessage const& message,
        BryptIdentifier::SharedContainer const& spBryptPeerIdentifier);

    TrackerKey PushRequest(
        std::weak_ptr<CBryptPeer> const& wpRequestor,
        CApplicationMessage const& message,
        std::set<BryptIdentifier::SharedContainer> const& spBryptPeerIdentifier);

    bool PushResponse(CApplicationMessage const& message);
    void ProcessFulfilledRequests();

private:
    using ResponseTrackingMap = std::unordered_map<TrackerKey, CResponseTracker>;

    TrackerKey KeyGenerator(std::string_view pack) const;

    ResponseTrackingMap m_awaiting;
};

//------------------------------------------------------------------------------------------------
