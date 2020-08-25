//------------------------------------------------------------------------------------------------
// File: Node.hpp
// Description:
//------------------------------------------------------------------------------------------------
#pragma once
//------------------------------------------------------------------------------------------------
#include "../Components/Command/Handler.hpp"
#include "../Components/Endpoints/EndpointTypes.hpp"
#include "../Configuration/Configuration.hpp"
#include "../Utilities/NetworkUtils.hpp"
#include "../Utilities/NodeUtils.hpp"
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
    class CMessageObject;
    class CObjectContainer;
}

namespace BryptIdentifier {
    class CContainer;
}

namespace Configuration {
    class CManager;
}

class CCommand;
class CConnection;
class CControl;
class CEndpointManager;
class CMessage;
class CMessageQueue;
class CPeerPersistor;
class CPeerWatcher;

class CNodeState;
class CAuthorityState;
class CCoordinatorState;
class CNetworkState;
class CSecurityState;
class CSensorState;

//------------------------------------------------------------------------------------------------

class CBryptNode {
public:
    // Constructors and Deconstructors
    CBryptNode(
        BryptIdentifier::CContainer const& identifier,
        std::shared_ptr<CEndpointManager> const& spEndpointManager,
        std::shared_ptr<CMessageQueue> const& spMessageQueue,
        std::shared_ptr<CPeerPersistor> const& spPeerPersistor,
        std::unique_ptr<Configuration::CManager> const& upConfigurationManager);

    void Startup();
    bool Shutdown();

    // Getter Functions
    std::weak_ptr<CNodeState> GetNodeState() const;
    std::weak_ptr<CAuthorityState> GetAuthorityState() const;
    std::weak_ptr<CCoordinatorState> GetCoordinatorState() const;
    std::weak_ptr<CNetworkState> GetNetworkState() const;
    std::weak_ptr<CSecurityState> GetSecurityState() const;
    std::weak_ptr<CSensorState> GetSensorState() const;

    std::weak_ptr<CEndpointManager> GetEndpointManager() const;
    std::weak_ptr<CMessageQueue> GetMessageQueue() const;
    std::weak_ptr<CPeerPersistor> GetPeerPersistor() const;
    std::weak_ptr<Await::CObjectContainer> GetAwaiting() const;

private:
    // Utility Functions
    std::float_t DetermineNodePower();  // Determine the node value to the network
    bool HasTechnologyType(Endpoints::TechnologyType technology);

    // Communication Functions
    bool ContactAuthority();    // Contact the central authority for some service
    bool NotifyAddressChange(); // Notify the cluster of some address change

    // Request Handlers
    void HandleQueueRequest(CMessage const& message);
    void ProcessFulfilledMessages();

    // Election Functions
    bool Election();    // Call for an election for cluster leader
    bool Transform();   // Transform the node's function in the cluster/network

    // Run Functions
    void Listen();

    // Private Variables
    std::shared_ptr<CNodeState> m_spNodeState;
    std::shared_ptr<CAuthorityState> m_spAuthorityState;
    std::shared_ptr<CCoordinatorState> m_spCoordinatorState;
    std::shared_ptr<CNetworkState> m_spNetworkState;
    std::shared_ptr<CSecurityState> m_spSecurityState;
    std::shared_ptr<CSensorState> m_spSensorState;

    std::shared_ptr<CEndpointManager> m_spEndpointManager;
    std::shared_ptr<CMessageQueue> m_spMessageQueue;
    std::shared_ptr<CPeerPersistor> m_spPeerPersistor;
    std::shared_ptr<Await::CObjectContainer> m_spAwaiting;
    Command::HandlerMap m_commandHandlers;
    std::shared_ptr<CPeerWatcher> m_spWatcher;

    bool m_initialized;
};

//------------------------------------------------------------------------------------------------