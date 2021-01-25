//------------------------------------------------------------------------------------------------
// File: BryptNode.hpp
// Description:
//------------------------------------------------------------------------------------------------
#pragma once
//------------------------------------------------------------------------------------------------
#include "BryptIdentifier/IdentifierTypes.hpp"
#include "Components/BryptPeer/BryptPeer.hpp"
#include "Components/Configuration/Configuration.hpp"
#include "Components/Handler/Handler.hpp"
#include "Components/Network/EndpointTypes.hpp"
//------------------------------------------------------------------------------------------------
#include <cstdio>
#include <cstdlib>
#include <cstdint>
#include <cmath>
#include <ctime>
#include <iostream>
#include <memory>
#include <mutex>
#include <vector>
//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
// Forward Declarations
//------------------------------------------------------------------------------------------------

namespace spdlog { class logger; }

namespace Await {
    class ResponseTracker;
    class TrackingManager;
}

namespace Configuration {
    class Manager;
}

class AuthorizedProcessor;
class EndpointManager;
class PeerManager;
class PeerPersistor;

class AuthorityState;
class CoordinatorState;
class NetworkState;
class NodeState;
class SecurityState;
class SensorState;

//------------------------------------------------------------------------------------------------

class BryptNode {
public:
    // Constructors and Deconstructors
    BryptNode(
        BryptIdentifier::SharedContainer const& spBryptIdentifier,
        std::shared_ptr<EndpointManager> const& spEndpointManager,
        std::shared_ptr<PeerManager> const& spPeerManager,
        std::shared_ptr<AuthorizedProcessor> const& spMessageProcessor,
        std::shared_ptr<PeerPersistor> const& spPeerPersistor,
        std::unique_ptr<Configuration::Manager> const& upConfigurationManager);

    void Startup();
    bool Shutdown();

    // Getter Functions
    std::weak_ptr<NodeState> GetNodeState() const;
    std::weak_ptr<CoordinatorState> GetCoordinatorState() const;
    std::weak_ptr<NetworkState> GetNetworkState() const;
    std::weak_ptr<SecurityState> GetSecurityState() const;
    std::weak_ptr<SensorState> GetSensorState() const;

    std::weak_ptr<EndpointManager> GetEndpointManager() const;
    std::weak_ptr<PeerManager> GetPeerManager() const;
    std::weak_ptr<PeerPersistor> GetPeerPersistor() const;
    std::weak_ptr<Await::TrackingManager> GetAwaitManager() const;

private:
    // Run Functions
    void StartLifecycle();

    // Lifecycle Functions
    void HandleIncomingMessage(AssociatedMessage const& associatedMessage);

    bool m_initialized;
    std::shared_ptr<spdlog::logger> m_spLogger;

    std::shared_ptr<NodeState> m_spNodeState;
    std::shared_ptr<CoordinatorState> m_spCoordinatorState;
    std::shared_ptr<NetworkState> m_spNetworkState;
    std::shared_ptr<SecurityState> m_spSecurityState;
    std::shared_ptr<SensorState> m_spSensorState;

    std::shared_ptr<EndpointManager> m_spEndpointManager;
    std::shared_ptr<PeerManager> m_spPeerManager;
    std::shared_ptr<AuthorizedProcessor> m_spMessageProcessor;
    std::shared_ptr<Await::TrackingManager> m_spAwaitManager;
    std::shared_ptr<PeerPersistor> m_spPeerPersistor;

    Handler::HandlerMap m_handlers;
};

//------------------------------------------------------------------------------------------------