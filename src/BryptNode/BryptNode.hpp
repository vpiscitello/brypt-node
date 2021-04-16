//----------------------------------------------------------------------------------------------------------------------
// File: BryptNode.hpp
// Description:
//----------------------------------------------------------------------------------------------------------------------
#pragma once
//----------------------------------------------------------------------------------------------------------------------
#include "RuntimePolicy.hpp"
#include "BryptIdentifier/IdentifierTypes.hpp"
#include "Components/Configuration/Configuration.hpp"
#include "Components/Handler/Handler.hpp"
#include "Components/Network/EndpointTypes.hpp"
#include "Components/Peer/Proxy.hpp"
//----------------------------------------------------------------------------------------------------------------------
#include <concepts>
#include <memory>
//----------------------------------------------------------------------------------------------------------------------

//----------------------------------------------------------------------------------------------------------------------
// Forward Declarations
//----------------------------------------------------------------------------------------------------------------------

namespace spdlog { class logger; }

namespace Await {
    class ResponseTracker;
    class TrackingManager;
}

namespace Configuration { class Manager; }
namespace Event { class Publisher; }
namespace Network { class Manager; }
namespace Peer { class Manager; }

class AuthorizedProcessor;
class PeerPersistor;

class AuthorityState;
class CoordinatorState;
class NetworkState;
class NodeState;
class SecurityState;
class SensorState;

//----------------------------------------------------------------------------------------------------------------------

class BryptNode final
{
public:
    // Runtime Handler {
    friend class IRuntimePolicy;
    // } Runtime Handler

    BryptNode(
        Node::SharedIdentifier const& spNodeIdentifier,
        std::shared_ptr<Event::Publisher> const& spEventPublisher,
        std::shared_ptr<Network::Manager> const& spNetworkManager,
        std::shared_ptr<Peer::Manager> const& spPeerManager,
        std::shared_ptr<AuthorizedProcessor> const& spMessageProcessor,
        std::shared_ptr<PeerPersistor> const& spPeerPersistor,
        std::unique_ptr<Configuration::Manager> const& upConfigurationManager);

    BryptNode(BryptNode const& other) = delete;
    BryptNode(BryptNode&& other) = default;
    BryptNode& operator=(BryptNode const& other) = delete;
    BryptNode& operator=(BryptNode&& other) = default;

    ~BryptNode() = default;

    template<typename RuntimePolicy = ForegroundRuntime> requires std::derived_from<RuntimePolicy, IRuntimePolicy>
    [[nodiscard]] bool Startup();
    [[nodiscard]] bool Shutdown();
    [[nodiscard]] bool IsActive() const;

    [[nodiscard]] std::weak_ptr<NodeState> GetNodeState() const;
    [[nodiscard]] std::weak_ptr<CoordinatorState> GetCoordinatorState() const;
    [[nodiscard]] std::weak_ptr<NetworkState> GetNetworkState() const;
    [[nodiscard]] std::weak_ptr<SecurityState> GetSecurityState() const;
    [[nodiscard]] std::weak_ptr<SensorState> GetSensorState() const;

    [[nodiscard]] std::weak_ptr<Event::Publisher> GetEventPublisher() const;
    [[nodiscard]] std::weak_ptr<Network::Manager> GetNetworkManager() const;
    [[nodiscard]] std::weak_ptr<Peer::Manager> GetPeerManager() const;
    [[nodiscard]] std::weak_ptr<PeerPersistor> GetPeerPersistor() const;
    [[nodiscard]] std::weak_ptr<Await::TrackingManager> GetAwaitManager() const;

private:
    bool StartComponents();

    bool m_initialized;
    std::shared_ptr<spdlog::logger> m_spLogger;

    std::shared_ptr<NodeState> m_spNodeState;
    std::shared_ptr<CoordinatorState> m_spCoordinatorState;
    std::shared_ptr<NetworkState> m_spNetworkState;
    std::shared_ptr<SecurityState> m_spSecurityState;
    std::shared_ptr<SensorState> m_spSensorState;

    std::shared_ptr<Event::Publisher> m_spEventPublisher;
    std::shared_ptr<Network::Manager> m_spNetworkManager;
    std::shared_ptr<Peer::Manager> m_spPeerManager;
    std::shared_ptr<AuthorizedProcessor> m_spMessageProcessor;
    std::shared_ptr<Await::TrackingManager> m_spAwaitManager;
    std::shared_ptr<PeerPersistor> m_spPeerPersistor;

    Handler::Map m_handlers;
    std::unique_ptr<IRuntimePolicy> m_upRuntime;
};

//----------------------------------------------------------------------------------------------------------------------

template<typename RuntimePolicy> requires std::derived_from<RuntimePolicy, IRuntimePolicy>
[[nodiscard]] bool BryptNode::Startup()
{
    if (!StartComponents()) { return false; }
    m_upRuntime = std::make_unique<RuntimePolicy>(*this);
    return m_upRuntime->Start();
}

//----------------------------------------------------------------------------------------------------------------------
