//------------------------------------------------------------------------------------------------
// File: Notifier.cpp 
// Description:
//-----------------------------------------------------------------------------------------------
#include "Notifier.hpp"
#include "../../BryptNode/CoordinatorState.hpp"
#include "../Endpoints/Endpoint.hpp"
#include "../../Utilities/Message.hpp"
//-----------------------------------------------------------------------------------------------

//-----------------------------------------------------------------------------------------------

CNotifier::CNotifier(
    NodeUtils::PortNumber port,
    std::weak_ptr<CCoordinatorState> const& wpCoordinatorState,
    std::weak_ptr<NodeUtils::EndpointMap> const& endpoints)
    : m_wpCoordinatorState(wpCoordinatorState)
    , m_wpConnections(wpConnections)
    , m_context(1)
    , m_publisher(m_context, ZMQ_PUB)
    , m_subscriber(m_context, ZMQ_SUB)
    , m_networkPrefix(std::string())
    , m_clusterPrefix(std::string())
{
    NodeUtils::printo("Setting up publisher socket on port " + port, NodeUtils::PrintType::Notifier);
    m_publisher.bind("tcp://*:" + port);
}

//-----------------------------------------------------------------------------------------------

bool CNotifier::Connect(NodeUtils::NetworkAddress const& ip, NodeUtils::PortNumber const& port)
{
    printo("Subscribing to peer at " + ip + ":" + port, NodeUtils::PrintType::Notifier);
    m_subscriber.connect("tcp://" + ip + ":" + port);

    m_networkPrefix = std::string(Notifier::NetworkPrefix.data(), Notifier::NetworkPrefix.size());

    m_clusterPrefix = std::string(Notifier::ClusterPrefix.data(), Notifier::ClusterPrefix.size());
    if (auto const spCoordinatorState = m_wpCoordinatorState.lock()) {
        m_clusterPrefix.append(std::to_string(spCoordinatorState->GetId()) + ":");
    }

    std::string nodePrefix(Notifier::NodePrefix.data(), Notifier::NodePrefix.size());
    if (auto const spCoordinatorState = m_wpCoordinatorState.lock()) {
        nodePrefix.append(std::to_string(spCoordinatorState->GetId()) + ":");
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

    // TODO: notify peers that cannot connect to the ZMQ publisher

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
    printo("Received: " + notification, NodeUtils::PrintType::Notifier);

    return notification;
}

//-----------------------------------------------------------------------------------------------

std::string CNotifier::getNotificationPrefix(
    NodeUtils::NotificationType type,
    std::optional<NodeUtils::NodeIdType> const& id)
{
    switch (type) {
        case NodeUtils::NotificationType::Network: return m_networkPrefix;
        case NodeUtils::NotificationType::Cluster: return m_clusterPrefix;
        case NodeUtils::NotificationType::Node: {
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
