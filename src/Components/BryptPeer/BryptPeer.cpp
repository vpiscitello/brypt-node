//------------------------------------------------------------------------------------------------
// File: BryptPeer.cpp
// Description: 
//------------------------------------------------------------------------------------------------
#include "BryptPeer.hpp"
#include "../../BryptIdentifier/ReservedIdentifiers.hpp"
#include "../../Interfaces/PeerMediator.hpp"
#include "../../Message/Message.hpp"
#include "../../Utilities/NetworkUtils.hpp"
//------------------------------------------------------------------------------------------------

CBryptPeer::CBryptPeer(
    BryptIdentifier::CContainer const& identifier,
    IPeerMediator* const pPeerMediator)
    : m_pPeerMediator(pPeerMediator)
    , m_dataMutex()
    , m_spBryptIdentifier()
    , m_location()
    , m_endpointsMutex()
    , m_endpoints()
{
    // A Brypt Peer must always be constructed with an identifier that can uniquely identify
    // the peer. 
    if (!identifier.IsValid() || ReservedIdentifiers::IsIdentifierReserved(identifier)) {
        throw std::runtime_error(
            "Cannot create a CBryptPeer with an invalid CBryptIdentifier");
    }

    m_spBryptIdentifier = std::make_shared<BryptIdentifier::CContainer const>(identifier);
}

//------------------------------------------------------------------------------------------------

BryptIdentifier::SharedContainer CBryptPeer::GetBryptIdentifier() const
{
    return m_spBryptIdentifier;
}

//------------------------------------------------------------------------------------------------

BryptIdentifier::InternalType CBryptPeer::GetInternalBryptIdentifier() const
{
    assert(m_spBryptIdentifier);
    return m_spBryptIdentifier->GetInternalRepresentation();
}

//------------------------------------------------------------------------------------------------

std::string CBryptPeer::GetLocation() const
{
    std::scoped_lock lock(m_dataMutex);
    return m_location;
}

//------------------------------------------------------------------------------------------------

void CBryptPeer::SetLocation(std::string_view location)
{
    std::scoped_lock lock(m_dataMutex);
    m_location = location;
}

//------------------------------------------------------------------------------------------------

void CBryptPeer::RegisterEndpoint(CEndpointRegistration const& registration)
{
    {
        std::scoped_lock lock(m_endpointsMutex);
        m_endpoints.emplace(registration.GetEndpointIdentifier(), registration);
    }

    // When an endpoint registers a connection with this peer, the mediator needs to notify the
    // observers that this peer has been connected to a new endpoint. 
    if (m_pPeerMediator) { 
        m_pPeerMediator->DispatchPeerStateChange(
            weak_from_this(),
            registration.GetEndpointIdentifier(),
            registration.GetEndpointTechnology(),
            ConnectionState::Connected);
    }
}

//------------------------------------------------------------------------------------------------

void CBryptPeer::RegisterEndpoint(
    Endpoints::EndpointIdType identifier,
    Endpoints::TechnologyType technology,
    MessageScheduler const& scheduler,
    std::string_view uri)
{
    {
        std::scoped_lock lock(m_endpointsMutex);
        m_endpoints.try_emplace(
            // Registered Endpoint Key
            identifier,
            // Registered Endpoint Arguments
            identifier, technology, scheduler, uri);
    }

    // When an endpoint registers a connection with this peer, the mediator needs to notify the
    // observers that this peer has been connected to a new endpoint. 
    if (m_pPeerMediator) {
        m_pPeerMediator->DispatchPeerStateChange(
            weak_from_this(), identifier, technology, ConnectionState::Connected);
    }
}

//------------------------------------------------------------------------------------------------

void CBryptPeer::WithdrawEndpoint(
    Endpoints::EndpointIdType identifier,
    Endpoints::TechnologyType technology)
{
    {
        std::scoped_lock lock(m_endpointsMutex);
        m_endpoints.erase(identifier);
    }

    // When an endpoint withdraws its registration from the peer, the mediator needs to notify
    // observer's that peer has been disconnected from that endpoint. 
    if (m_pPeerMediator) {
        m_pPeerMediator->DispatchPeerStateChange(
            weak_from_this(), identifier, technology, ConnectionState::Disconnected);
    }
}

//------------------------------------------------------------------------------------------------

bool CBryptPeer::IsEndpointRegistered(Endpoints::EndpointIdType identifier) const
{
    std::scoped_lock lock(m_endpointsMutex);
    auto const itr = m_endpoints.find(identifier);
    return (itr != m_endpoints.end());
}

//------------------------------------------------------------------------------------------------

std::optional<std::string> CBryptPeer::GetRegisteredEntry(
    Endpoints::EndpointIdType identifier) const
{
    std::scoped_lock lock(m_endpointsMutex);
    if (auto const& itr = m_endpoints.find(identifier); itr != m_endpoints.end()) {
        auto const& [key, endpoint] = *itr;
        if (auto const entry = endpoint.GetEntry(); !entry.empty()) {
            return entry;
        }
    }

    return {};
}

//------------------------------------------------------------------------------------------------

std::uint32_t CBryptPeer::RegisteredEndpointCount() const
{
    std::scoped_lock lock(m_endpointsMutex);
    return m_endpoints.size();
}

//------------------------------------------------------------------------------------------------

bool CBryptPeer::IsActive() const
{
    std::scoped_lock lock(m_endpointsMutex);
    return (m_endpoints.size() != 0);
}

//------------------------------------------------------------------------------------------------

bool CBryptPeer::ScheduleSend(CMessage const& message) const
{
    std::scoped_lock lock(m_endpointsMutex);
    auto const& context = message.GetMessageContext();
    if (auto const itr = m_endpoints.find(context.GetEndpointIdentifier());
        itr != m_endpoints.end()) {
        auto const& [identifier, endpoint] = *itr;
        auto const& scheduler = endpoint.GetScheduler();
        if (scheduler) {
            return scheduler(message);
        }
    }

    return false;
}

//------------------------------------------------------------------------------------------------
