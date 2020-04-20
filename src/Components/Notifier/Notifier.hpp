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

class CNotifier {
public:
    CNotifier(
        std::weak_ptr<CCoordinatorState> const& wpCoordinatorState,
        std::weak_ptr<EndpointMap> const& wpEndpoints);

    // Passthrough for send function of the endpoint type
    bool Send(
        CMessage const& message,
        NodeUtils::NotificationType type,
        std::optional<NodeUtils::NodeIdType> const& id = {});

private:
    std::string GetNotificationPrefix(
        NodeUtils::NotificationType type,
        std::optional<NodeUtils::NodeIdType> const& id);

    std::weak_ptr<CCoordinatorState> m_wpCoordinatorState;
    std::weak_ptr<EndpointMap> m_wpEndpoints;

    std::string m_networkPrefix;
    std::string m_clusterPrefix;
};

//------------------------------------------------------------------------------------------------
