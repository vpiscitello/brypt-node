//------------------------------------------------------------------------------------------------
// File: BryptNode.hpp
// Description:
//------------------------------------------------------------------------------------------------
#pragma once
//------------------------------------------------------------------------------------------------
#include "../Components/BryptPeer/BryptPeer.hpp"
#include "../Components/Command/Handler.hpp"
#include "../Components/Endpoints/EndpointTypes.hpp"
#include "../Configuration/Configuration.hpp"
#include "../Message/Message.hpp"
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

namespace BryptIdentifier {
    class CContainer;
    using SharedContainer = std::shared_ptr<CContainer const>;
}

namespace Configuration {
    class CManager;
}

class CCommand;
class CConnection;
class CControl;
class CEndpointManager;
class CMessage;
class CMessageCollector;
class CPeerManager;
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
        BryptIdentifier::SharedContainer const& spBryptIdentifier,
        std::shared_ptr<CEndpointManager> const& spEndpointManager,
        std::shared_ptr<CPeerManager> const& spPeerManager,
        std::shared_ptr<CMessageCollector> const& spMessageCollector,
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
    std::weak_ptr<CPeerManager> GetPeerManager() const;
    std::weak_ptr<CMessageCollector> GetMessageCollector() const;
    std::weak_ptr<CPeerPersistor> GetPeerPersistor() const;
    std::weak_ptr<Await::CTrackingManager> GetAwaitManager() const;

private:
    // Run Functions
    void StartLifecycle();

    // Lifecycle Functions
    void HandleIncomingMessage(AssociatedMessage const& associatedMessage);

    // Utility Functions
    std::float_t DetermineNodePower();  // Determine the node value to the network
    bool HasTechnologyType(Endpoints::TechnologyType technology);

    // Communication Functions
    bool ContactAuthority();    // Contact the central authority for some service
    bool NotifyAddressChange(); // Notify the cluster of some address change

    // Election Functions
    bool Election();    // Call for an election for cluster leader
    bool Transform();   // Transform the node's function in the cluster/network

    // Private Variables
    std::shared_ptr<CNodeState> m_spNodeState;
    std::shared_ptr<CAuthorityState> m_spAuthorityState;
    std::shared_ptr<CCoordinatorState> m_spCoordinatorState;
    std::shared_ptr<CNetworkState> m_spNetworkState;
    std::shared_ptr<CSecurityState> m_spSecurityState;
    std::shared_ptr<CSensorState> m_spSensorState;

    std::shared_ptr<CEndpointManager> m_spEndpointManager;
    std::shared_ptr<CPeerManager> m_spPeerManager;
    std::shared_ptr<CMessageCollector> m_spMessageCollector ;
    std::shared_ptr<Await::CTrackingManager> m_spAwaitManager;
    std::shared_ptr<CPeerWatcher> m_spWatcher;

    Command::HandlerMap m_handlers;

    std::shared_ptr<CPeerPersistor> m_spPeerPersistor;

    bool m_initialized;
};

//------------------------------------------------------------------------------------------------