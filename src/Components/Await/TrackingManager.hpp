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

namespace Peer { class Proxy; }

class ApplicationMessage;

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
        std::weak_ptr<Peer::Proxy> const& wpRequestor,
        ApplicationMessage const& message,
        Node::SharedIdentifier const& spIdentifier);

    TrackerKey PushRequest(
        std::weak_ptr<Peer::Proxy> const& wpRequestor,
        ApplicationMessage const& message,
        std::set<Node::SharedIdentifier> const& spIdentifier);

    bool PushResponse(ApplicationMessage const& message);
    void ProcessFulfilledRequests();

private:
    using ResponseTrackingMap = std::unordered_map<TrackerKey, ResponseTracker>;

    TrackerKey KeyGenerator(std::string_view pack) const;

    ResponseTrackingMap m_awaiting;
    std::shared_ptr<spdlog::logger> m_logger;
};

//----------------------------------------------------------------------------------------------------------------------
