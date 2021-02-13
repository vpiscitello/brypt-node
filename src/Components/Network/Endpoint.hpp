//------------------------------------------------------------------------------------------------
// File: Endpoint.hpp
// Description: Defines a set of communication methods for use on varying types of communication
// protocols. Currently supports TCP sockets.
//------------------------------------------------------------------------------------------------
#pragma once
//------------------------------------------------------------------------------------------------
#include "EndpointTypes.hpp"
#include "EndpointIdentifier.hpp"
#include "Protocol.hpp"
#include "BryptIdentifier/BryptIdentifier.hpp"
#include "Components/Configuration/Configuration.hpp"
#include "Interfaces/EndpointMediator.hpp"
#include "Interfaces/PeerMediator.hpp"
//------------------------------------------------------------------------------------------------
#include <memory>
#include <string>
#include <string_view>
//------------------------------------------------------------------------------------------------

class BryptPeer;

namespace Network {
    class IEndpoint;
    class Address;
}

namespace Network::LoRa { class Endpoint; }
namespace Network::TCP { class Endpoint; }

//------------------------------------------------------------------------------------------------
namespace Network::Endpoint {
//------------------------------------------------------------------------------------------------

std::unique_ptr<IEndpoint> Factory(
    Protocol protocol,
    Operation operation,
    IEndpointMediator const* const pEndpointMediator,
    IPeerMediator* const pPeerMediator);

//------------------------------------------------------------------------------------------------
} // Network::Network namespace
//------------------------------------------------------------------------------------------------

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

    virtual void ScheduleBind(BindingAddress const& address) = 0;
    
    virtual void ScheduleConnect(std::string_view uri) = 0;
    virtual void ScheduleConnect(
        std::string_view uri,
        BryptIdentifier::SharedContainer const& spIdentifier) = 0;

	virtual bool ScheduleSend(
        BryptIdentifier::Container const& destination, std::string_view message) = 0;

    Network::Endpoint::Identifier GetEndpointIdentifier() const;
    Operation GetOperation() const;

    void RegisterMediator(IEndpointMediator const* const pMediator);
    void RegisterMediator(IPeerMediator* const pMediator);

protected: 
    std::shared_ptr<BryptPeer> LinkPeer(
        BryptIdentifier::Container const& identifier, RemoteAddress const& address) const;
    
    Network::Endpoint::Identifier const m_identifier;
	Operation const m_operation;
    Network::Protocol const m_protocol;

    BindingAddress m_binding;

    IEndpointMediator const* m_pEndpointMediator;
    IPeerMediator* m_pPeerMediator;
};

//------------------------------------------------------------------------------------------------
