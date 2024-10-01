//----------------------------------------------------------------------------------------------------------------------
// File: Connect.hpp
// Description: 
//----------------------------------------------------------------------------------------------------------------------
#pragma once
//----------------------------------------------------------------------------------------------------------------------
#include "MessageHandler.hpp"
#include "Components/Configuration/Options.hpp"
#include "Components/Identifier/IdentifierTypes.hpp"
#include "Components/Message/Payload.hpp"
#include "Interfaces/ConnectProtocol.hpp"
#include "Interfaces/EndpointMediator.hpp"
//----------------------------------------------------------------------------------------------------------------------
#include <memory>
#include <string>
#include <string_view>
//----------------------------------------------------------------------------------------------------------------------

class BootstrapService;
class NodeState;

namespace spdlog { class logger; }

namespace Peer { class Proxy; class ProxyStore; }
namespace Message { class Context; }
namespace Node { class ServiceProvider; }
namespace Network { class Manager; }

//----------------------------------------------------------------------------------------------------------------------
namespace Route::Fundamental::Connect {
//----------------------------------------------------------------------------------------------------------------------

class DiscoveryHandler;
class DiscoveryProtocol;

//----------------------------------------------------------------------------------------------------------------------
} // Route::Fundamental::Connect namespace
//----------------------------------------------------------------------------------------------------------------------

class Route::Fundamental::Connect::DiscoveryHandler : public Route::IMessageHandler
{
public:
    static constexpr std::string_view Path = "/connect/discovery";

    DiscoveryHandler() = default;

    // IMessageHandler{
    [[nodiscard]] virtual bool OnFetchServices(std::shared_ptr<Node::ServiceProvider> const& spServiceProvider) override;
    [[nodiscard]] virtual bool OnMessage(Message::Application::Parcel const& message, Peer::Action::Next& next) override;
    // }IMessageHandler

private:
    [[nodiscard]] bool GenerateResponsePayload();

    std::weak_ptr<NodeState> m_wpNodeState;
    std::weak_ptr<BootstrapService> m_wpBootstrapService;
    std::weak_ptr<Network::Manager> m_wpNetworkManager;
    std::weak_ptr<Peer::ProxyStore> m_wpProxyStore;
    Message::Payload m_response;
};

//----------------------------------------------------------------------------------------------------------------------

class Route::Fundamental::Connect::DiscoveryProtocol : public IConnectProtocol
{
public:
    DiscoveryProtocol();

    DiscoveryProtocol(DiscoveryProtocol&&) = delete;
    DiscoveryProtocol(DiscoveryProtocol const&) = delete;
    void operator=(DiscoveryProtocol const&) = delete;

    [[nodiscard]] bool CompileRequest(std::shared_ptr<Node::ServiceProvider> const& spServiceProvider);

    // IConnectProtocol {
    virtual bool SendRequest(
        std::shared_ptr<Peer::Proxy> const& spProxy, Message::Context const& context) const override;
    // } IConnectProtocol

private:
    [[nodiscard]] bool GenerateRequestPayload();

    std::weak_ptr<NodeState> m_wpNodeState;
    std::weak_ptr<BootstrapService> m_wpBootstrapService;
    std::weak_ptr<Network::Manager> m_wpNetworkManager;
    std::weak_ptr<Peer::ProxyStore> m_wpProxyStore;
    Message::Payload m_payload;
    std::shared_ptr<spdlog::logger> m_logger;
};

//---------------------------------------------------------------------------------------------------------------------- 