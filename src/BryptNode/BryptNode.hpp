//------------------------------------------------------------------------------------------------
// File: Node.hpp
// Description:
//------------------------------------------------------------------------------------------------
#pragma once
//------------------------------------------------------------------------------------------------
#include "../Configuration/Configuration.hpp"
#include "../Utilities/NodeUtils.hpp"
#include "../Components/Command/Handler.hpp"
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
namespace Await {
    class CMessageObject;
    class CObjectContainer;
}

class CCommand;
class CConnection;
class CControl;
class CMessage;
class CMessageQueue;
class CNotifier;
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
    explicit CBryptNode(Configuration::TSettings const& settings);
    ~CBryptNode();

    void Startup();
    bool Shutdown();

    std::shared_ptr<CConnection> SetupFullConnection(
        NodeUtils::NodeIdType id,
        NodeUtils::PortNumber const& port,
        NodeUtils::TechnologyType technology);

    void NotifyConnection(NodeUtils::NodeIdType const& id);

    // Getter Functions
    std::weak_ptr<CNodeState> GetNodeState() const;
    std::weak_ptr<CAuthorityState> GetAuthorityState() const;
    std::weak_ptr<CCoordinatorState> GetCoordinatorState() const;
    std::weak_ptr<CNetworkState> GetNetworkState() const;
    std::weak_ptr<CSecurityState> GetSecurityState() const;
    std::weak_ptr<CSensorState> GetSensorState() const;
    std::weak_ptr<CMessageQueue> GetMessageQueue() const;
    std::weak_ptr<Await::CObjectContainer> GetAwaiting() const;
    std::weak_ptr<NodeUtils::ConnectionMap> GetConnections() const;
    std::optional<std::weak_ptr<CConnection>> GetConnection(NodeUtils::NodeIdType const& id) const;
    std::weak_ptr<CControl> GetControl() const;
    std::weak_ptr<CNotifier> GetNotifier() const;

private:
    // Utility Functions
    std::float_t determineNodePower();  // Determine the node value to the network
    NodeUtils::TechnologyType determineConnectionType();   // Determine the connection method for a particular transmission
    NodeUtils::TechnologyType determineBestConnectionType();   // Determine the best connection type the node has
    bool hasTechnologyType(NodeUtils::TechnologyType technology);
    bool addConnection(std::unique_ptr<CConnection> const& connection);

    // Communication Functions
    void initialContact();
    void joinCoordinator();
    bool contactAuthority();    // Contact the central authority for some service
    bool notifyAddressChange(); // Notify the cluster of some address change

    // Request Handlers
    void handleControlRequest(std::string const& message);
    void handleNotification(std::string const& message);
    void handleQueueRequest(CMessage const& message);
    void processFulfilledMessages();

    // Election Functions
    bool election();    // Call for an election for cluster leader
    bool transform();   // Transform the node's function in the cluster/network

    // Run Functions
    void listen();  // Open a socket to listening for network commands
    void connect();

    // Private Variables
    std::shared_ptr<CNodeState> m_spNodeState;
    std::shared_ptr<CAuthorityState> m_spAuthorityState;
    std::shared_ptr<CCoordinatorState> m_spCoordinatorState;
    std::shared_ptr<CNetworkState> m_spNetworkState;
    std::shared_ptr<CSecurityState> m_spSecurityState;
    std::shared_ptr<CSensorState> m_spSensorState;

    // These classes should be implemented a threadsafe manner
    std::shared_ptr<CMessageQueue> m_spQueue;
    std::shared_ptr<Await::CObjectContainer> m_spAwaiting;

    // Commands of the node
    Command::HandlerMap m_commandHandlers;

    // Connection of the node
    std::shared_ptr<NodeUtils::ConnectionMap> m_spConnections;
    std::shared_ptr<CControl> m_spControl;
    std::shared_ptr<CNotifier> m_spNotifier;
    std::shared_ptr<CPeerWatcher> m_spWatcher;

    bool m_initialized;
};

//------------------------------------------------------------------------------------------------