//----------------------------------------------------------------------------------------------------------------------
// File: Endpoint.hpp
// Description: Defines a set of communication methods for use on varying types of communication
// protocols. Currently supports TCP sockets.
//----------------------------------------------------------------------------------------------------------------------
#pragma once
//----------------------------------------------------------------------------------------------------------------------
#include "EndpointTypes.hpp"
#include "EndpointIdentifier.hpp"
#include "Protocol.hpp"
#include "MessageScheduler.hpp"
#include "BryptIdentifier/BryptIdentifier.hpp"
#include "BryptMessage/ShareablePack.hpp"
#include "Components/Event/Events.hpp"
#include "Interfaces/EndpointMediator.hpp"
#include "Interfaces/PeerMediator.hpp"
//----------------------------------------------------------------------------------------------------------------------
#include <memory>
#include <string>
#include <string_view>
//----------------------------------------------------------------------------------------------------------------------

namespace Event { class Publisher; }

namespace Peer { class Proxy; }

namespace Network {
    class IEndpoint;
    class Address;
}

namespace Network::LoRa { class Endpoint; }
namespace Network::TCP { class Endpoint; }

//----------------------------------------------------------------------------------------------------------------------
namespace Network::Endpoint {
//----------------------------------------------------------------------------------------------------------------------

std::unique_ptr<IEndpoint> Factory(
    Protocol protocol,
    Operation operation,
    std::shared_ptr<Event::Publisher> const& spEventPublisher,
    IEndpointMediator* const pEndpointMediator,
    IPeerMediator* const pPeerMediator);

//----------------------------------------------------------------------------------------------------------------------
} // Network::Endpoint namespace
//----------------------------------------------------------------------------------------------------------------------

class Network::IEndpoint
{
public:
    IEndpoint(Protocol protocol, Operation operation, std::shared_ptr<Event::Publisher> const& spEventPublisher);
    virtual ~IEndpoint() = default;

    [[nodiscard]] virtual Protocol GetProtocol() const = 0;
    [[nodiscard]] virtual std::string GetScheme() const = 0;
    [[nodiscard]] virtual BindingAddress GetBinding() const = 0;

    virtual void Startup() = 0;
	[[nodiscard]] virtual bool Shutdown() = 0;
    [[nodiscard]] virtual bool IsActive() const = 0;

    [[nodiscard]] virtual bool ScheduleBind(BindingAddress const& binding) = 0;
    [[nodiscard]] virtual bool ScheduleConnect(RemoteAddress const& address) = 0;
    [[nodiscard]] virtual bool ScheduleConnect(RemoteAddress&& address) = 0;
    [[nodiscard]] virtual bool ScheduleConnect(RemoteAddress&& address, Node::SharedIdentifier const& spIdentifier) = 0;
	[[nodiscard]] virtual bool ScheduleSend(Node::Identifier const& destination, std::string&& message) = 0;
    [[nodiscard]] virtual bool ScheduleSend(
        Node::Identifier const& identifier, Message::ShareablePack const& spSharedPack) = 0;
    [[nodiscard]] virtual bool ScheduleSend(Node::Identifier const& identifier, MessageVariant&& message) = 0;

    [[nodiscard]] Endpoint::Identifier GetEndpointIdentifier() const;
    [[nodiscard]] Operation GetOperation() const;

    void RegisterMediator(IEndpointMediator* const pMediator);
    void RegisterMediator(IPeerMediator* const pMediator);

protected: 
    using ShutdownCause = Event::Message<Event::Type::EndpointStopped>::Cause;

    std::shared_ptr<Peer::Proxy> LinkPeer(Node::Identifier const& identifier, RemoteAddress const& address) const;

    void OnStarted() const;
    void OnStopped() const;
    void OnBindFailed(BindingAddress const& binding) const;
    void OnConnectFailed(RemoteAddress const& address) const;
    void OnUnexpectedError() const;

    void OnBindingUpdated(BindingAddress const& binding);

    void SetShutdownCause(ShutdownCause cause) const;
    
    Endpoint::Identifier const m_identifier;
    Network::Protocol const m_protocol;
	Operation const m_operation;

    BindingAddress m_binding;

    std::shared_ptr<Event::Publisher> m_spEventPublisher;

    IEndpointMediator* m_pEndpointMediator;
    IPeerMediator* m_pPeerMediator;

private:
    // Note: This is mutable because we shouldn't prevent capturing an error that occurs in a const method. 
    mutable std::optional<ShutdownCause> m_optShutdownCause;
};

//----------------------------------------------------------------------------------------------------------------------
