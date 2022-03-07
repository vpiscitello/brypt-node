//----------------------------------------------------------------------------------------------------------------------
// File: TrackingService.hpp
// Description:
//----------------------------------------------------------------------------------------------------------------------
#pragma once
//----------------------------------------------------------------------------------------------------------------------
#include "Definitions.hpp"
#include "Tracker.hpp"
#include "BryptIdentifier/BryptIdentifier.hpp"
#include "Utilities/InvokeContext.hpp"
//----------------------------------------------------------------------------------------------------------------------
#include <functional>
#include <set>
#include <string_view>
#include <unordered_map>
#include <vector>
//----------------------------------------------------------------------------------------------------------------------

namespace spdlog { class logger; }

namespace Node { class Provider; }
namespace Peer { class Proxy; }
namespace Scheduler { class Delegate; class Registrar; }
namespace Message::Application { class Parcel; }

//----------------------------------------------------------------------------------------------------------------------
namespace Awaitable {
//----------------------------------------------------------------------------------------------------------------------

class TrackingService;

//----------------------------------------------------------------------------------------------------------------------
} // Awaitable namespace
//----------------------------------------------------------------------------------------------------------------------

//----------------------------------------------------------------------------------------------------------------------
// Description:
//----------------------------------------------------------------------------------------------------------------------
class Awaitable::TrackingService
{
public:
    TrackingService(
        std::shared_ptr<Scheduler::Registrar> const& spRegistrar,
        std::shared_ptr<Node::ServiceProvider> const& spProvider);
    ~TrackingService();

    [[nodiscard]] std::optional<TrackerKey> StageDeferred(
        std::weak_ptr<Peer::Proxy> const& wpRequestor,
        std::vector<Node::SharedIdentifier> const& identifiers,
        Message::Application::Parcel const& deferred,
        Message::Application::Builder& builder);

    [[nodiscard]] bool Process(Message::Application::Parcel const& message);
    [[nodiscard]] bool Process(TrackerKey key, Node::Identifier const& identifier, std::string_view data);
    
    void CheckTrackers();
    [[nodiscard]] std::size_t Execute();

private:
    struct KeyHasher {
       [[nodiscard]] std::size_t operator()(TrackerKey const& key) const;
    };

    using ActiveTrackers = std::unordered_map<TrackerKey, std::unique_ptr<ITracker>, KeyHasher>;

    [[nodiscard]] std::optional<TrackerKey> GenerateKey(Node::Identifier const& identifier) const;
    void OnTrackerReady(TrackerKey key, std::unique_ptr<ITracker>&& upTracker);

    std::shared_ptr<Scheduler::Delegate> m_spDelegate;
    ActiveTrackers m_trackers;
    std::vector<std::unique_ptr<ITracker>> m_ready;
    std::shared_ptr<spdlog::logger> m_logger;
};

//----------------------------------------------------------------------------------------------------------------------
