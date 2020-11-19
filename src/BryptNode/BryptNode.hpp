//------------------------------------------------------------------------------------------------
// File: BryptNode.hpp
// Description:
//------------------------------------------------------------------------------------------------
#pragma once
//------------------------------------------------------------------------------------------------
#include "../BryptIdentifier/IdentifierTypes.hpp"
#include "../Components/BryptPeer/BryptPeer.hpp"
#include "../Components/Command/Handler.hpp"
#include "../Components/Endpoints/EndpointTypes.hpp"
#include "../Configuration/Configuration.hpp"
#include "../BryptMessage/ApplicationMessage.hpp"
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

namespace Await {
    class CResponseTracker;
    class CTrackingManager;
}

namespace Configuration {
    class CManager;
}

class CAuthorizedProcessor;
class CEndpointManager;
class CPeerManager;
class CPeerPersistor;

class CAuthorityState;
class CCoordinatorState;
class CNetworkState;
class CNodeState;
class CSecurityState;
class CSensorState;

//------------------------------------------------------------------------------------------------

class CBryptNode {
public:
    // Constructors and Deconstructors
    CBryptNode(
        BryptIdentifier::SharedContainer const& spBryptIdentifier,
        std::shared_ptr<CEndpointManager> const& spEndpointManager,
        std::shared_ptr<CPeerManager> const& spPeerManager,
        std::shared_ptr<CAuthorizedProcessor> const& spMessageProcessor,
        std::shared_ptr<CPeerPersistor> const& spPeerPersistor,
        std::unique_ptr<Configuration::CManager> const& upConfigurationManager);

    void Startup();
    bool Shutdown();

    // Getter Functions
    std::weak_ptr<CNodeState> GetNodeState() const;
    std::weak_ptr<CCoordinatorState> GetCoordinatorState() const;
    std::weak_ptr<CNetworkState> GetNetworkState() const;
    std::weak_ptr<CSecurityState> GetSecurityState() const;
    std::weak_ptr<CSensorState> GetSensorState() const;

    std::weak_ptr<CEndpointManager> GetEndpointManager() const;
    std::weak_ptr<CPeerManager> GetPeerManager() const;
    std::weak_ptr<CPeerPersistor> GetPeerPersistor() const;
    std::weak_ptr<Await::CTrackingManager> GetAwaitManager() const;

private:
    // Run Functions
    void StartLifecycle();

    // Lifecycle Functions
    void HandleIncomingMessage(AssociatedMessage const& associatedMessage);

    bool m_initialized;

    std::shared_ptr<CNodeState> m_spNodeState;
    std::shared_ptr<CCoordinatorState> m_spCoordinatorState;
    std::shared_ptr<CNetworkState> m_spNetworkState;
    std::shared_ptr<CSecurityState> m_spSecurityState;
    std::shared_ptr<CSensorState> m_spSensorState;

    std::shared_ptr<CEndpointManager> m_spEndpointManager;
    std::shared_ptr<CPeerManager> m_spPeerManager;
    std::shared_ptr<CAuthorizedProcessor> m_spMessageProcessor;
    std::shared_ptr<Await::CTrackingManager> m_spAwaitManager;
    std::shared_ptr<CPeerPersistor> m_spPeerPersistor;

    Command::HandlerMap m_handlers;

};

//------------------------------------------------------------------------------------------------