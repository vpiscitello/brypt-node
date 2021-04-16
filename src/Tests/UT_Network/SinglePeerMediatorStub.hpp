//----------------------------------------------------------------------------------------------------------------------
// File: SinglePeerMediatorStub.hpp
// Description: An IPeerMediator stub implementation that allows endpoint tests to test single
// point connection. Requires an IMessageSink stub to set the receiver on the linked Peer::Proxy
//----------------------------------------------------------------------------------------------------------------------
#pragma once
//----------------------------------------------------------------------------------------------------------------------
#include "BryptIdentifier/IdentifierTypes.hpp"
#include "Components/Network/EndpointIdentifier.hpp"
#include "Components/Network/Protocol.hpp"
#include "Interfaces/MessageSink.hpp"
#include "Interfaces/PeerMediator.hpp"
#include "Interfaces/PeerObserver.hpp"
//----------------------------------------------------------------------------------------------------------------------
#include <memory>
//----------------------------------------------------------------------------------------------------------------------

namespace Peer { class Proxy; }

//----------------------------------------------------------------------------------------------------------------------

class SinglePeerMediatorStub : public IPeerMediator
{
public:
    SinglePeerMediatorStub(
        Node::SharedIdentifier const& spNodeIdentifier, IMessageSink* const pMessageSink);

    // IPeerMediator {
    virtual void RegisterObserver(IPeerObserver* const observer) override;
    virtual void UnpublishObserver(IPeerObserver* const observer) override;

    virtual OptionalRequest DeclareResolvingPeer(
        Network::RemoteAddress const& address, Node::SharedIdentifier const& spIdentifier = nullptr) override;

    virtual void UndeclareResolvingPeer(Network::RemoteAddress const& address) override;

    virtual std::shared_ptr<Peer::Proxy> LinkPeer(
        Node::Identifier const& identifier, Network::RemoteAddress const& address) override;

    virtual void DispatchPeerStateChange(
        std::weak_ptr<Peer::Proxy> const& wpPeerProxy,
        Network::Endpoint::Identifier identifier,
        Network::Protocol protocol,
        ConnectionState change) override;
    // } IPeerMediator

    std::shared_ptr<Peer::Proxy> GetPeer() const;

private:
    Node::SharedIdentifier m_spNodeIdentifier;
    std::shared_ptr<Peer::Proxy> m_spPeer;
    IMessageSink* const m_pMessageSink;
};

//----------------------------------------------------------------------------------------------------------------------