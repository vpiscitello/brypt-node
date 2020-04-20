//------------------------------------------------------------------------------------------------
// File: Notifier.cpp 
// Description:
//-----------------------------------------------------------------------------------------------
#include "Notifier.hpp"
#include "../Endpoints/Endpoint.hpp"
#include "../Endpoints/EndpointTypes.hpp"
#include "../../BryptNode/CoordinatorState.hpp"
#include "../../Utilities/Message.hpp"
//-----------------------------------------------------------------------------------------------

//-----------------------------------------------------------------------------------------------

CNotifier::CNotifier(
    std::weak_ptr<CCoordinatorState> const& wpCoordinatorState,
    std::weak_ptr<EndpointMap> const& wpEndpoints)
    : m_wpCoordinatorState(wpCoordinatorState)
    , m_wpEndpoints(wpEndpoints)
    , m_networkPrefix(std::string())
    , m_clusterPrefix(std::string())
{
}

//-----------------------------------------------------------------------------------------------

bool CNotifier::Send(
    CMessage const& message,
    NodeUtils::NotificationType type,
    std::optional<NodeUtils::NodeIdType> const& id)
{
    std::string const& messageBuffer = message.GetPack();
    std::string const notification = GetNotificationPrefix(type, id) + messageBuffer;

    // TODO: notify peers that cannot connect to the ZMQ publisher
    
    return true;
}

//-----------------------------------------------------------------------------------------------

std::string CNotifier::GetNotificationPrefix(
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
