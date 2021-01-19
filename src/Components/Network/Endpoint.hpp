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
#include "Utilities/NetworkUtils.hpp"
#include "Utilities/NodeUtils.hpp"
//------------------------------------------------------------------------------------------------
#include <memory>
#include <string>
#include <string_view>
//------------------------------------------------------------------------------------------------

class BryptPeer;

namespace Network { class IEndpoint; }
namespace Network::LoRa { class Endpoint; }
namespace Network::TCP { class Endpoint; }

//------------------------------------------------------------------------------------------------
namespace Network::Endpoint {
//------------------------------------------------------------------------------------------------

std::unique_ptr<IEndpoint> Factory(
    Protocol protocol,
    std::string_view interface,
    Operation operation,
    IEndpointMediator const* const pEndpointMediator,
    IPeerMediator* const pPeerMediator);

//------------------------------------------------------------------------------------------------
} // Network::Endpoint namespace
//------------------------------------------------------------------------------------------------

class Network::IEndpoint
{
public:
    IEndpoint(
        std::string_view interface,
        Operation operation,
        Network::Protocol protocol = Network::Protocol::Invalid);

    virtual ~IEndpoint() = default;

    virtual Network::Protocol GetProtocol() const = 0;
    virtual std::string GetProtocolString() const = 0;
    virtual std::string GetEntry() const = 0;
    virtual std::string GetURI() const = 0;

    virtual void Startup() = 0;
	virtual bool Shutdown() = 0;
    virtual bool IsActive() const = 0;

    virtual void ScheduleBind(std::string_view binding) = 0;
    virtual void ScheduleConnect(std::string_view entry) = 0;
    virtual void ScheduleConnect(
        BryptIdentifier::SharedContainer const& spIdentifier, std::string_view entry) = 0;
	virtual bool ScheduleSend(
        BryptIdentifier::Container const& destination, std::string_view message) = 0;

    Network::Endpoint::Identifier GetEndpointIdentifier() const;
    Operation GetOperation() const;

    void RegisterMediator(IEndpointMediator const* const pMediator);
    void RegisterMediator(IPeerMediator* const pMediator);

protected: 
    std::shared_ptr<BryptPeer> LinkPeer(
        BryptIdentifier::Container const& identifier,
        std::string_view uri = "") const;

    Network::Endpoint::Identifier const m_identifier;
    std::string m_interface;
	Operation const m_operation;
    Network::Protocol const m_protocol;

    std::string m_entry;

    IEndpointMediator const* m_pEndpointMediator;
    IPeerMediator* m_pPeerMediator;
};

//------------------------------------------------------------------------------------------------
