//------------------------------------------------------------------------------------------------
// File: SinglePeerMediatorStub.cpp
// Description: 
//------------------------------------------------------------------------------------------------
#include "SinglePeerMediatorStub.hpp"
#include "../../Components/BryptPeer/BryptPeer.hpp"
//------------------------------------------------------------------------------------------------

CSinglePeerMediatorStub::CSinglePeerMediatorStub(IMessageSink* const pMessageSink)
    : m_spBryptPeer()
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

std::shared_ptr<CBryptPeer> CSinglePeerMediatorStub::LinkPeer(
    BryptIdentifier::CContainer const& identifier)
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