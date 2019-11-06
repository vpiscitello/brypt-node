//------------------------------------------------------------------------------------------------
// File: Notifier.cpp 
// Description:
//-----------------------------------------------------------------------------------------------
#include "Notifier.hpp"
#include "../Connection/Connection.hpp"
#include "../../State.hpp"
#include "../../Utilities/Message.hpp"
//-----------------------------------------------------------------------------------------------

//-----------------------------------------------------------------------------------------------

CNotifier::CNotifier(
    std::shared_ptr<CState> const& state,
    std::weak_ptr<NodeUtils::ConnectionMap> const& connections)
    : m_state(state)
    , m_connections(connections)
    , m_context(1)
    , m_publisher(m_context, ZMQ_PUB)
    , m_subscriber(m_context, ZMQ_SUB)
    , m_networkPrefix(std::string())
    , m_clusterPrefix(std::string())
{
    if (auto const selfState = m_state->GetSelfState().lock()) {
        NodeUtils::PortNumber const port = selfState->GetPublisherPort();
        NodeUtils::printo("Setting up publisher socket on port " + port, NodeUtils::PrintType::NOTIFIER);
        m_publisher.bind("tcp://*:" + port);
    }
}

//-----------------------------------------------------------------------------------------------

bool CNotifier::Connect(NodeUtils::IPv4Address const& ip, NodeUtils::PortNumber const& port)
{
    printo("Subscribing to peer at " + ip + ":" + port, NodeUtils::PrintType::NOTIFIER);
    m_subscriber.connect("tcp://" + ip + ":" + port);

    m_networkPrefix = std::string(Notifier::NetworkPrefix.data(), Notifier::NetworkPrefix.size());

    m_clusterPrefix = std::string(Notifier::ClusterPrefix.data(), Notifier::ClusterPrefix.size());
    if (auto const coordinatorState = m_state->GetCoordinatorState().lock()) {
        m_clusterPrefix.append(std::to_string(coordinatorState->GetId()) + ":");
    }

    std::string nodePrefix(Notifier::NodePrefix.data(), Notifier::NodePrefix.size());
    if (auto const selfState = m_state->GetCoordinatorState().lock()) {
        nodePrefix.append(std::to_string(selfState->GetId()) + ":");
    }

    m_subscriber.setsockopt(ZMQ_SUBSCRIBE, m_networkPrefix.c_str(), m_networkPrefix.size());
    m_subscriber.setsockopt(ZMQ_SUBSCRIBE, m_clusterPrefix.c_str(), m_clusterPrefix.size());
    m_subscriber.setsockopt(ZMQ_SUBSCRIBE, nodePrefix.c_str(), nodePrefix.size());

    return m_subscribed = true;
}

//-----------------------------------------------------------------------------------------------

bool CNotifier::Send(
    CMessage const& message,
    NodeUtils::NotificationType type,
    std::optional<NodeUtils::NodeIdType> const& id)
{
    std::string const& messageBuffer = message.GetPack();
    std::string const notification = getNotificationPrefix(type, id) + messageBuffer;

    // ZMQ Publisher Scope
    {
        zmq::message_t zmqMessage(notification.size());
        memcpy(zmqMessage.data(), notification.c_str(), notification.size());
        m_publisher.send(zmqMessage, zmq::send_flags::none);
    }

    // Non-ZMQ Publishing Scope
    {
        auto const connections = m_connections.lock();
        NodeUtils::TechnologyType currentConnectionType;
        for (auto itr = connections->begin(); itr != connections->end(); ++itr) {
            currentConnectionType = itr->second->GetInternalType();
            if(currentConnectionType != NodeUtils::TechnologyType::DIRECT) {
               itr->second->Send(message); 
            }
        }
    }

    return true;
}

//-----------------------------------------------------------------------------------------------

std::optional<std::string> CNotifier::Receive()
{
    if (!m_subscribed) {
        return {};
    }

    zmq::message_t message;
    auto const result = m_subscriber.recv(message, zmq::recv_flags::dontwait);
    if (!result) {
        return {};
    }

    std::string const notification = std::string(static_cast<char *>(message.data()), message.size());
    printo("Received: " + notification, NodeUtils::PrintType::NOTIFIER) ;

    return notification;
}

//-----------------------------------------------------------------------------------------------

std::string CNotifier::getNotificationPrefix(
    NodeUtils::NotificationType type,
    std::optional<NodeUtils::NodeIdType> const& id)
{
    switch (type) {
        case NodeUtils::NotificationType::NETWORK: return m_networkPrefix;
        case NodeUtils::NotificationType::CLUSTER: return m_clusterPrefix;
        case NodeUtils::NotificationType::NODE: {
            if (id) {
                std::string nodePrefix(Notifier::NodePrefix.data(), Notifier::NodePrefix.size());
                nodePrefix.append(std::to_string(*id) + ":");
                return nodePrefix;
            } else {
                return "";
            }
        }
        default: return "";
    }
}

//-----------------------------------------------------------------------------------------------
