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

namespace Scheduler {
    class Delegate;
    class Service;
}

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
    explicit TrackingManager(std::shared_ptr<Scheduler::Service> const& spScheduler);

    TrackerKey PushRequest(
        std::weak_ptr<Peer::Proxy> const& wpRequestor,
        ApplicationMessage const& message,
        Node::SharedIdentifier const& spIdentifier);

    TrackerKey PushRequest(
        std::weak_ptr<Peer::Proxy> const& wpRequestor,
        ApplicationMessage const& message,
        std::set<Node::SharedIdentifier> const& spIdentifier);

    bool PushResponse(ApplicationMessage const& message);
    [[nodiscard]] std::size_t ProcessFulfilledRequests();

private:
    using ResponseTrackingMap = std::unordered_map<TrackerKey, ResponseTracker>;

    TrackerKey KeyGenerator(std::string_view pack) const;

    std::shared_ptr<Scheduler::Delegate> m_spDelegate;
    ResponseTrackingMap m_awaiting;
    std::shared_ptr<spdlog::logger> m_logger;
};

//----------------------------------------------------------------------------------------------------------------------
