//------------------------------------------------------------------------------------------------
// File: SinglePeerMediatorStub.hpp
// Description: An IPeerMediator stub implementation that allows endpoint tests to test single
// point connection. Requires an IMessageSink stub to set the receiver on the linked CBryptPeer
//------------------------------------------------------------------------------------------------
#pragma once
//------------------------------------------------------------------------------------------------
#include "../../BryptIdentifier/IdentifierTypes.hpp"
#include "../../Interfaces/MessageSink.hpp"
#include "../../Interfaces/PeerMediator.hpp"
#include "../../Interfaces/PeerObserver.hpp"
#include "../../Components/Endpoints/EndpointIdentifier.hpp"
#include "../../Components/Endpoints/TechnologyType.hpp"
//------------------------------------------------------------------------------------------------
#include <memory>
//------------------------------------------------------------------------------------------------

class CBryptPeer;

//------------------------------------------------------------------------------------------------

class CSinglePeerMediatorStub : public IPeerMediator
{
public:
    CSinglePeerMediatorStub(
        BryptIdentifier::SharedContainer const& spBryptIdentifier,
        IMessageSink* const pMessageSink);

    // IPeerMediator {
    virtual void RegisterObserver(IPeerObserver* const observer) override;
    virtual void UnpublishObserver(IPeerObserver* const observer) override;

    virtual OptionalRequest DeclareResolvingPeer(std::string_view uri) override;

    virtual std::shared_ptr<CBryptPeer> LinkPeer(
        BryptIdentifier::CContainer const& identifier,
        std::string_view uri = "") override;

    virtual void DispatchPeerStateChange(
        std::weak_ptr<CBryptPeer> const& wpBryptPeer,
        Endpoints::EndpointIdType identifier,
        Endpoints::TechnologyType technology,
        ConnectionState change) override;
    // } IPeerMediator

private:
    BryptIdentifier::SharedContainer m_spBryptIdentifier;
    std::shared_ptr<CBryptPeer> m_spBryptPeer;
    IMessageSink* const m_pMessageSink;

};

//------------------------------------------------------------------------------------------------