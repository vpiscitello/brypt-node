//------------------------------------------------------------------------------------------------
// File: Notifier.hpp
// Description:
//------------------------------------------------------------------------------------------------
#pragma once
//------------------------------------------------------------------------------------------------
#include "../../Utilities/NodeUtils.hpp"
//------------------------------------------------------------------------------------------------
#include <memory>
#include <optional>
#include <string_view>
#include <zmq.hpp>
//------------------------------------------------------------------------------------------------
class CState;
class CMessage;
class CConnection;
//------------------------------------------------------------------------------------------------
namespace Notifier {
//------------------------------------------------------------------------------------------------
constexpr std::string_view NetworkPrefix = "network.all";
constexpr std::string_view ClusterPrefix = "cluster.";
constexpr std::string_view NodePrefix = "node."; 
//------------------------------------------------------------------------------------------------
} // Notifier namespace
//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------

class CNotifier {
public:
    CNotifier(
        std::shared_ptr<CState> const& state,
        std::weak_ptr<NodeUtils::ConnectionMap> const& connections);

    ~CNotifier()
    {
        m_publisher.close();
        m_subscriber.close();
        m_context.close();
    };

    bool Connect(NodeUtils::IPv4Address const& ip, NodeUtils::IPv4Address const& port);
    // Passthrough for send function of the connection type
    // Update to target sub clusters and nodes in the prefix
    bool Send(
        CMessage const& message,
        NodeUtils::NotificationType const& type,
        std::optional<std::string> const& id = {});
    // Receive for requests, if a request is received recv it and then return the message string
    std::optional<std::string> Receive();

private:
    std::string getNotificationPrefix(
        NodeUtils::NotificationType const& type,
        std::optional<std::string> const& id);

    std::shared_ptr<CState> m_state;
    std::weak_ptr<NodeUtils::ConnectionMap> m_connections;

    zmq::context_t m_context;
    zmq::socket_t m_publisher;
    zmq::socket_t m_subscriber;

    std::string m_networkPrefix;
    std::string m_clusterPrefix;

    bool m_subscribed;
};

//------------------------------------------------------------------------------------------------
