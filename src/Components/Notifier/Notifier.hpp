//------------------------------------------------------------------------------------------------
// File: Notifier.hpp
// Description:
//------------------------------------------------------------------------------------------------
#pragma once
//------------------------------------------------------------------------------------------------
#include "../Endpoints/EndpointTypes.hpp"
#include "../../Utilities/NodeUtils.hpp"
//------------------------------------------------------------------------------------------------
#include <memory>
#include <optional>
#include <string_view>
#include <zmq.hpp>
//------------------------------------------------------------------------------------------------
class CNodeState;
class CCoordinatorState;
class CMessage;
class CEndpoint;
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
        NodeUtils::PortNumber port,
        std::weak_ptr<CCoordinatorState> const& wpCoordinatorState,
        std::weak_ptr<EndpointMap> const& wpEndpoints);

    ~CNotifier()
    {
        m_publisher.close();
        m_subscriber.close();
        m_context.close();
    };

    bool Connect(NodeUtils::NetworkAddress const& ip, NodeUtils::PortNumber const& port);
    // Passthrough for send function of the endpoint type
    // Update to target sub clusters and nodes in the prefix
    bool Send(
        CMessage const& message,
        NodeUtils::NotificationType type,
        std::optional<NodeUtils::NodeIdType> const& id = {});
    // Receive for requests, if a request is received recv it and then return the message string
    std::optional<std::string> Receive();

private:
    std::string getNotificationPrefix(
        NodeUtils::NotificationType type,
        std::optional<NodeUtils::NodeIdType> const& id);

    std::weak_ptr<CCoordinatorState> m_wpCoordinatorState;
    std::weak_ptr<EndpointMap> m_wpEndpoints;

    zmq::context_t m_context;
    zmq::socket_t m_publisher;
    zmq::socket_t m_subscriber;

    std::string m_networkPrefix;
    std::string m_clusterPrefix;

    bool m_subscribed;
};

//------------------------------------------------------------------------------------------------
