//----------------------------------------------------------------------------------------------------------------------
// File: PeerMediator.hpp
// Description: 
//----------------------------------------------------------------------------------------------------------------------
#pragma once
//----------------------------------------------------------------------------------------------------------------------
#include "BryptIdentifier/IdentifierTypes.hpp"
#include "Components/Network/ConnectionState.hpp"
#include "Components/Network/EndpointIdentifier.hpp"
//----------------------------------------------------------------------------------------------------------------------
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
//----------------------------------------------------------------------------------------------------------------------

namespace Network { class RemoteAddress; }
namespace Peer { class Proxy; }

class IPeerObserver;

//----------------------------------------------------------------------------------------------------------------------

class IPeerMediator
{
public:
    using OptionalRequest = std::optional<std::string>;

    virtual ~IPeerMediator() = default;

    virtual void RegisterObserver(IPeerObserver* const observer) = 0;
    virtual void UnpublishObserver(IPeerObserver* const observer) = 0;

    virtual OptionalRequest DeclareResolvingPeer(
        Network::RemoteAddress const& address, Node::SharedIdentifier const& spIdentifier = nullptr) = 0;

    virtual void RescindResolvingPeer(Network::RemoteAddress const& address) = 0;

    virtual std::shared_ptr<Peer::Proxy> LinkPeer(
        Node::Identifier const& identifier, Network::RemoteAddress const& address) = 0;

    virtual void DispatchConnectionState(
        std::shared_ptr<Peer::Proxy> const& spPeerProxy,
        Network::Endpoint::Identifier identifier,
        Network::RemoteAddress const& address, 
        Network::Connection::State change) = 0;
};

//----------------------------------------------------------------------------------------------------------------------