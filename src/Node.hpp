//------------------------------------------------------------------------------------------------
// File: Node.hpp
// Description:
//------------------------------------------------------------------------------------------------
#pragma once
//------------------------------------------------------------------------------------------------
#include "Utilities/NodeUtils.hpp"
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

class CState;
//------------------------------------------------------------------------------------------------

class CNode {
public:
    // Constructors and Deconstructors
    explicit CNode(NodeUtils::TOptions const& options);
    ~CNode() {
        m_control.reset();
        m_notifier.reset();
        m_watcher.reset();
        m_connections.reset();
        m_awaiting.reset();
        m_queue.reset();
        m_state.reset();
    };

    void Startup();

    std::shared_ptr<CConnection> SetupFullConnection(
        NodeUtils::NodeIdType const& peerId,
        NodeUtils::PortNumber const& port,
        NodeUtils::TechnologyType const technology);

    void NotifyConnection(NodeUtils::NodeIdType const& id);

    // Getter Functions
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
    bool hasTechnologyType(NodeUtils::TechnologyType const& technology);
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
    std::shared_ptr<CState> m_state;

    // These classes should be implemented a threadsafe manner
    std::shared_ptr<CMessageQueue> m_queue;
    std::shared_ptr<Await::CObjectContainer> m_awaiting;

    // Commands of the node
    NodeUtils::CommandMap m_commands;

    // Connection of the node
    std::shared_ptr<NodeUtils::ConnectionMap> m_connections;
    std::shared_ptr<CControl> m_control;
    std::shared_ptr<CNotifier> m_notifier;
    std::shared_ptr<CPeerWatcher> m_watcher;

    bool m_initialized;
};

struct ThreadArgs {
    CNode* node;
    NodeUtils::TOptions* opts;
};

void * ConnectionHandler(void *);
