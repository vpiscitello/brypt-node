//----------------------------------------------------------------------------------------------------------------------
// File: Information.hpp
// Description:
//----------------------------------------------------------------------------------------------------------------------
#pragma once
//----------------------------------------------------------------------------------------------------------------------
#include "MessageHandler.hpp"
//----------------------------------------------------------------------------------------------------------------------
#include <string_view>
//----------------------------------------------------------------------------------------------------------------------

class CoordinatorState;
class NetworkState;
class NodeState;
class IPeerCache;

namespace Awaitable { class TrackingService; }
namespace Network { class Manager; }

//----------------------------------------------------------------------------------------------------------------------
namespace Route::Fundamental::Information {
//----------------------------------------------------------------------------------------------------------------------

class NodeHandler;
class FetchNodeHandler;

//----------------------------------------------------------------------------------------------------------------------
} // Route::Fundamental::Information namespace
//----------------------------------------------------------------------------------------------------------------------

class Route::Fundamental::Information::NodeHandler : public Route::IMessageHandler
{
public:
    static constexpr std::string_view Path = "/info/node";

    NodeHandler() = default;

    // IMessageHandler{
    [[nodiscard]] virtual bool OnFetchServices(std::shared_ptr<Node::ServiceProvider> const& spServiceProvider) override;
    [[nodiscard]] virtual bool OnMessage(Message::Application::Parcel const& message, Peer::Action::Next& next) override;
    // }IMessageHandler

private:
    std::weak_ptr<NodeState> m_wpNodeState;
    std::weak_ptr<CoordinatorState> m_wpCoordinatorState;
    std::weak_ptr<NetworkState> m_wpNetworkState;
    std::weak_ptr<Network::Manager> m_wpNetworkManager;
    std::weak_ptr<IPeerCache> m_wpPeerCache;
};

//----------------------------------------------------------------------------------------------------------------------

class Route::Fundamental::Information::FetchNodeHandler : public Route::IMessageHandler
{
public:
    static constexpr std::string_view Path = "/info/fetch/node";

    FetchNodeHandler() = default;

    // IMessageHandler{
    [[nodiscard]] virtual bool OnFetchServices(std::shared_ptr<Node::ServiceProvider> const& spServiceProvider) override;
    [[nodiscard]] virtual bool OnMessage(Message::Application::Parcel const& message, Peer::Action::Next& next) override;
    // }IMessageHandler

private:
    std::weak_ptr<NodeState> m_wpNodeState;
    std::weak_ptr<CoordinatorState> m_wpCoordinatorState;
    std::weak_ptr<NetworkState> m_wpNetworkState;
    std::weak_ptr<Network::Manager> m_wpNetworkManager;
    std::weak_ptr<IPeerCache> m_wpPeerCache;
};

//----------------------------------------------------------------------------------------------------------------------
