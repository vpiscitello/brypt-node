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
#include "BryptIdentifier/BryptIdentifier.hpp"
#include "Interfaces/EndpointMediator.hpp"
#include "Interfaces/PeerMediator.hpp"
//----------------------------------------------------------------------------------------------------------------------
#include <memory>
#include <string>
#include <string_view>
//----------------------------------------------------------------------------------------------------------------------

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
    IEndpointMediator const* const pEndpointMediator,
    IPeerMediator* const pPeerMediator);

//----------------------------------------------------------------------------------------------------------------------
} // Network::Network namespace
//----------------------------------------------------------------------------------------------------------------------

class Network::IEndpoint
{
public:
    IEndpoint(
        Operation operation,
        Network::Protocol protocol = Network::Protocol::Invalid);

    virtual ~IEndpoint() = default;

    virtual Network::Protocol GetProtocol() const = 0;
    virtual std::string GetScheme() const = 0;
    virtual BindingAddress GetBinding() const = 0;

    virtual void Startup() = 0;
	virtual bool Shutdown() = 0;
    virtual bool IsActive() const = 0;

    virtual void ScheduleBind(BindingAddress const& binding) = 0;
    
    virtual void ScheduleConnect(RemoteAddress const& address) = 0;
    virtual void ScheduleConnect(RemoteAddress&& address) = 0;
    virtual void ScheduleConnect(
        RemoteAddress&& address,
        Node::SharedIdentifier const& spIdentifier) = 0;

	virtual bool ScheduleSend(
        Node::Identifier const& destination, std::string_view message) = 0;

    Network::Endpoint::Identifier GetEndpointIdentifier() const;
    Operation GetOperation() const;

    void RegisterMediator(IEndpointMediator const* const pMediator);
    void RegisterMediator(IPeerMediator* const pMediator);

protected: 
    std::shared_ptr<Peer::Proxy> LinkPeer(
        Node::Identifier const& identifier, RemoteAddress const& address) const;
    
    Network::Endpoint::Identifier const m_identifier;
	Operation const m_operation;
    Network::Protocol const m_protocol;

    BindingAddress m_binding;

    IEndpointMediator const* m_pEndpointMediator;
    IPeerMediator* m_pPeerMediator;
};

//----------------------------------------------------------------------------------------------------------------------
