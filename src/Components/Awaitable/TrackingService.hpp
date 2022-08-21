//----------------------------------------------------------------------------------------------------------------------
// File: TrackingService.hpp
// Description:
//----------------------------------------------------------------------------------------------------------------------
#pragma once
//----------------------------------------------------------------------------------------------------------------------
#include "Definitions.hpp"
#include "Tracker.hpp"
#include "BryptIdentifier/BryptIdentifier.hpp"
#include "Components/Scheduler/Tasks.hpp"
#include "Utilities/InvokeContext.hpp"
//----------------------------------------------------------------------------------------------------------------------
#include <functional>
#include <mutex>
#include <set>
#include <string_view>
#include <unordered_map>
#include <vector>
//----------------------------------------------------------------------------------------------------------------------

namespace spdlog { class logger; }

namespace Node { class ServiceProvider; }
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
    static constexpr auto CheckInterval = Scheduler::Interval{ 4 };
    
    using Correlator = std::function<bool(Node::SharedIdentifier const&)>;
    using Correlatable = std::pair<TrackerKey, Correlator>;
    
    explicit TrackingService(std::shared_ptr<Scheduler::Registrar> const& spRegistrar);
    ~TrackingService();

    [[nodiscard]] std::optional<TrackerKey> StageRequest(
        std::weak_ptr<Peer::Proxy> const& wpRequestee,
        Peer::Action::OnResponse const& onResponse,
        Peer::Action::OnError const& onError,
        Message::Application::Builder& builder);

    [[nodiscard]] std::optional<Correlatable> StageRequest(
        Node::Identifier const& self,
        std::size_t expected,
        Peer::Action::OnResponse const& onResponse,
        Peer::Action::OnError const& onError);

    [[nodiscard]] std::optional<TrackerKey> StageDeferred(
        std::weak_ptr<Peer::Proxy> const& wpRequestor,
        std::vector<Node::SharedIdentifier> const& identifiers,
        Message::Application::Parcel const& deferred,
        Message::Application::Builder& builder);

    void Cancel(Awaitable::TrackerKey const& key);

    [[nodiscard]] bool Process(Message::Application::Parcel&& message);
    [[nodiscard]] bool Process(TrackerKey key, Node::Identifier const& identifier, Message::Payload&& data);
    
    [[nodiscard]] std::size_t Waiting() const;
    [[nodiscard]] std::size_t Ready() const;

    [[nodiscard]] std::size_t Execute();

private:
    struct KeyHasher {
       [[nodiscard]] std::size_t operator()(TrackerKey const& key) const;
    };

    using TrackerContainer = std::unordered_map<TrackerKey, std::shared_ptr<ITracker>, KeyHasher>;

    void CheckTrackers();
    [[nodiscard]] std::optional<TrackerKey> GenerateKey(Node::Identifier const& identifier) const;

    std::shared_ptr<Scheduler::Delegate> m_spDelegate;
    mutable std::mutex m_mutex;
    TrackerContainer m_trackers;
    TrackerContainer m_ready;
    std::shared_ptr<spdlog::logger> m_logger;
};

//----------------------------------------------------------------------------------------------------------------------
