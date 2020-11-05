//------------------------------------------------------------------------------------------------
// File: SinglePeerMediatorStub.cpp
// Description: 
//------------------------------------------------------------------------------------------------
#include "SinglePeerMediatorStub.hpp"
#include "../../BryptMessage/ApplicationMessage.hpp"
#include "../../Components/BryptPeer/BryptPeer.hpp"
//------------------------------------------------------------------------------------------------
#include <cassert>
//------------------------------------------------------------------------------------------------

CSinglePeerMediatorStub::CSinglePeerMediatorStub(
    BryptIdentifier::SharedContainer const& spBryptIdentifier,
    IMessageSink* const pMessageSink)
    : m_spBryptIdentifier(spBryptIdentifier)
    , m_spBryptPeer()
    , m_pMessageSink(pMessageSink)
{
}

//------------------------------------------------------------------------------------------------

void CSinglePeerMediatorStub::RegisterObserver([[maybe_unused]] IPeerObserver* const observer)
{
}

//------------------------------------------------------------------------------------------------

void CSinglePeerMediatorStub::UnpublishObserver([[maybe_unused]] IPeerObserver* const observer)
{
}

//------------------------------------------------------------------------------------------------

CSinglePeerMediatorStub::OptionalRequest CSinglePeerMediatorStub::DeclareResolvingPeer(
    [[maybe_unused]] std::string_view uri)
{
    auto const optConnectRequest = CApplicationMessage::Builder()
        .SetSource(*m_spBryptIdentifier)
        .SetCommand(Command::Type::Connect, 0)
        .SetPayload("Connection Request")
        .ValidatedBuild();
    assert(optConnectRequest);

    return optConnectRequest->GetPack();
}

//------------------------------------------------------------------------------------------------

std::shared_ptr<CBryptPeer> CSinglePeerMediatorStub::LinkPeer(
    BryptIdentifier::CContainer const& identifier,
    [[maybe_unused]] std::string_view uri)
{
    m_spBryptPeer = std::make_shared<CBryptPeer>(identifier, this);
    m_spBryptPeer->SetReceiver(m_pMessageSink);
    return m_spBryptPeer;
}

//------------------------------------------------------------------------------------------------

void CSinglePeerMediatorStub::DispatchPeerStateChange(
    [[maybe_unused]] std::weak_ptr<CBryptPeer> const& wpBryptPeer,
    [[maybe_unused]] Endpoints::EndpointIdType identifier,
    [[maybe_unused]] Endpoints::TechnologyType technology,
    [[maybe_unused]] ConnectionState change)
{
}

//------------------------------------------------------------------------------------------------