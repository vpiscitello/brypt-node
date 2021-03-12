//----------------------------------------------------------------------------------------------------------------------
// File: TrackingManager.hpp
// Description:
//----------------------------------------------------------------------------------------------------------------------
#pragma once
//----------------------------------------------------------------------------------------------------------------------
#include "AwaitDefinitions.hpp"
#include "ResponseTracker.hpp"
#include "BryptIdentifier/BryptIdentifier.hpp"
//----------------------------------------------------------------------------------------------------------------------
#include <set>
#include <string_view>
#include <unordered_map>
#include <vector>
//----------------------------------------------------------------------------------------------------------------------

namespace spdlog { class logger; }

class ApplicationMessage;
class BryptPeer;

//----------------------------------------------------------------------------------------------------------------------
namespace Await {
//----------------------------------------------------------------------------------------------------------------------

class TrackingManager;

//----------------------------------------------------------------------------------------------------------------------
} // Await namespace
//----------------------------------------------------------------------------------------------------------------------

//----------------------------------------------------------------------------------------------------------------------
// Description:
//----------------------------------------------------------------------------------------------------------------------
class Await::TrackingManager
{
public:
    TrackingManager();

    TrackerKey PushRequest(
        std::weak_ptr<BryptPeer> const& wpRequestor,
        ApplicationMessage const& message,
        BryptIdentifier::SharedContainer const& spBryptPeerIdentifier);

    TrackerKey PushRequest(
        std::weak_ptr<BryptPeer> const& wpRequestor,
        ApplicationMessage const& message,
        std::set<BryptIdentifier::SharedContainer> const& spBryptPeerIdentifier);

    bool PushResponse(ApplicationMessage const& message);
    void ProcessFulfilledRequests();

private:
    using ResponseTrackingMap = std::unordered_map<TrackerKey, ResponseTracker>;

    TrackerKey KeyGenerator(std::string_view pack) const;

    ResponseTrackingMap m_awaiting;
    std::shared_ptr<spdlog::logger> m_spLogger;
};

//----------------------------------------------------------------------------------------------------------------------
