//------------------------------------------------------------------------------------------------
// File: SinglePeerMediatorStub.hpp
// Description: An IPeerMediator stub implementation that allows endpoint tests to test single
// point connection. Requires an IMessageSink stub to set the receiver on the linked BryptPeer
//------------------------------------------------------------------------------------------------
#pragma once
//------------------------------------------------------------------------------------------------
#include "BryptIdentifier/IdentifierTypes.hpp"
#include "Components/Network/EndpointIdentifier.hpp"
#include "Components/Network/Protocol.hpp"
#include "Interfaces/MessageSink.hpp"
#include "Interfaces/PeerMediator.hpp"
#include "Interfaces/PeerObserver.hpp"
//------------------------------------------------------------------------------------------------
#include <memory>
//------------------------------------------------------------------------------------------------

class BryptPeer;

//------------------------------------------------------------------------------------------------

class SinglePeerMediatorStub : public IPeerMediator
{
public:
    SinglePeerMediatorStub(
        BryptIdentifier::SharedContainer const& spBryptIdentifier,
        IMessageSink* const pMessageSink);

    // IPeerMediator {
    virtual void RegisterObserver(IPeerObserver* const observer) override;
    virtual void UnpublishObserver(IPeerObserver* const observer) override;

    virtual OptionalRequest DeclareResolvingPeer(
        Network::RemoteAddress const& address,
        BryptIdentifier::SharedContainer const& spIdentifier = nullptr) override;

    virtual void UndeclareResolvingPeer(Network::RemoteAddress const& address) override;

    virtual std::shared_ptr<BryptPeer> LinkPeer(
        BryptIdentifier::Container const& identifier,
        Network::RemoteAddress const& address) override;

    virtual void DispatchPeerStateChange(
        std::weak_ptr<BryptPeer> const& wpBryptPeer,
        Network::Endpoint::Identifier identifier,
        Network::Protocol protocol,
        ConnectionState change) override;
    // } IPeerMediator

    std::shared_ptr<BryptPeer> GetPeer() const;

private:
    BryptIdentifier::SharedContainer m_spBryptIdentifier;
    std::shared_ptr<BryptPeer> m_spBryptPeer;
    IMessageSink* const m_pMessageSink;
};

//------------------------------------------------------------------------------------------------