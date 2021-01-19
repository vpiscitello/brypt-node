//------------------------------------------------------------------------------------------------
// File: BryptPeer.cpp
// Description: 
//------------------------------------------------------------------------------------------------
#include "BryptPeer.hpp"
#include "BryptIdentifier/BryptIdentifier.hpp"
#include "BryptIdentifier/ReservedIdentifiers.hpp"
#include "BryptMessage/ApplicationMessage.hpp"
#include "BryptMessage/MessageContext.hpp"
#include "Components/Security/SecurityState.hpp"
#include "Components/Security/SecurityMediator.hpp"
#include "Interfaces/PeerMediator.hpp"
#include "Utilities/NetworkUtils.hpp"
//------------------------------------------------------------------------------------------------

BryptPeer::BryptPeer(
    BryptIdentifier::Container const& identifier,
    IPeerMediator* const pPeerMediator)
    : m_pPeerMediator(pPeerMediator)
    , m_dataMutex()
    , m_spBryptIdentifier()
    , m_statistics()
    , m_mediatorMutex()
    , m_upSecurityMediator()
    , m_endpointsMutex()
    , m_endpoints()
    , m_receiverMutex()
    , m_pMessageSink()
{
    // A Brypt Peer must always be constructed with an identifier that can uniquely identify
    // the peer. 
    if (!identifier.IsValid() ||
        ReservedIdentifiers::IsIdentifierReserved(identifier)) [[unlikely]] {
        throw std::runtime_error(
            "Error creating Brypt Peer with an invalid identifier!");
    }

    m_spBryptIdentifier = std::make_shared<BryptIdentifier::Container const>(identifier);
}

//------------------------------------------------------------------------------------------------

BryptPeer::~BryptPeer()
{
}

//------------------------------------------------------------------------------------------------

BryptIdentifier::SharedContainer BryptPeer::GetBryptIdentifier() const
{
    std::scoped_lock lock(m_dataMutex);
    return m_spBryptIdentifier;
}

//------------------------------------------------------------------------------------------------

BryptIdentifier::Internal::Type BryptPeer::GetInternalIdentifier() const
{
    std::scoped_lock lock(m_dataMutex);
    assert(m_spBryptIdentifier);
    return m_spBryptIdentifier->GetInternalRepresentation();
}

//------------------------------------------------------------------------------------------------

std::uint32_t BryptPeer::GetSentCount() const
{
    std::scoped_lock lock(m_dataMutex);
    return m_statistics.GetSentCount();
}

//------------------------------------------------------------------------------------------------

std::uint32_t BryptPeer::GetReceivedCount() const
{
    std::scoped_lock lock(m_dataMutex);
    return m_statistics.GetReceivedCount();
}

//------------------------------------------------------------------------------------------------

void BryptPeer::SetReceiver(IMessageSink* const pMessageSink)
{
    std::scoped_lock lock(m_receiverMutex);
    m_pMessageSink = pMessageSink;
}

//------------------------------------------------------------------------------------------------

bool BryptPeer::ScheduleReceive(
    Network::Endpoint::Identifier identifier, std::string_view buffer)
{
    {
        std::scoped_lock lock(m_dataMutex);
        m_statistics.IncrementReceivedCount();
    }

    std::scoped_lock lock(m_endpointsMutex);
    if (auto const itr = m_endpoints.find(identifier); itr != m_endpoints.end()) [[likely]] {
        auto const& [key, registration] = *itr;
        auto const& context = registration.GetMessageContext();

        // Forward the message through the message sink
        std::scoped_lock lock(m_receiverMutex);
        if (m_pMessageSink) [[likely]] {
            return m_pMessageSink->CollectMessage(weak_from_this(), context, buffer);
        }
    }

    return false;
}

//------------------------------------------------------------------------------------------------

bool BryptPeer::ScheduleReceive(
    Endpoints::EndpointIdType identifier, Message::Buffer const& buffer)
{
    {
        std::scoped_lock lock(m_dataMutex);
        m_statistics.IncrementReceivedCount();
    }

    std::scoped_lock lock(m_endpointsMutex);
    if (auto const itr = m_endpoints.find(identifier); itr != m_endpoints.end()) [[likely]] {
        auto const& [key, registration] = *itr;
        auto const& context = registration.GetMessageContext();

        // Forward the message through the message sink
        std::scoped_lock lock(m_receiverMutex);
        if (m_pMessageSink) [[likely]] {
            return m_pMessageSink->CollectMessage(weak_from_this(), context, buffer);
        }
    }

    return false;
}

//------------------------------------------------------------------------------------------------

bool BryptPeer::ScheduleSend(
    Endpoints::EndpointIdType identifier, std::string_view const& message) const
{
    {
        std::scoped_lock lock(m_dataMutex);
        m_statistics.IncrementSentCount();
    }

    std::scoped_lock lock(m_endpointsMutex);
    if (auto const itr = m_endpoints.find(identifier); itr != m_endpoints.end()) [[likely]] {
        auto const& [key, endpoint] = *itr;
        auto const& scheduler = endpoint.GetScheduler();
        if (m_spBryptIdentifier && scheduler) {
            return scheduler(*m_spBryptIdentifier, message);
        }
    }

    return false;
}

//------------------------------------------------------------------------------------------------

void BryptPeer::RegisterEndpoint(EndpointRegistration const& registration)
{
    {
        std::scoped_lock lock(m_endpointsMutex);
        auto [itr, result] = m_endpoints.try_emplace(
            registration.GetEndpointIdentifier(), registration);

        if (m_upSecurityMediator) [[likely]] {
            m_upSecurityMediator->BindSecurityContext(itr->second.GetWritableMessageContext());
        }     
    }

    // When an endpoint registers a connection with this peer, the mediator needs to notify the
    // observers that this peer has been connected to a new endpoint. 
    if (m_pPeerMediator) [[likely]] { 
        m_pPeerMediator->DispatchPeerStateChange(
            weak_from_this(),
            registration.GetEndpointIdentifier(),
            registration.GetEndpointTechnology(),
            ConnectionState::Connected);
    }
}

//------------------------------------------------------------------------------------------------

void BryptPeer::RegisterEndpoint(
    Endpoints::EndpointIdType identifier,
    Endpoints::TechnologyType technology,
    MessageScheduler const& scheduler,
    std::string_view uri)
{
    {
        std::scoped_lock lock(m_endpointsMutex);
        auto [itr, result] = m_endpoints.try_emplace(
            // Registered Endpoint Key
            identifier,
            // Registered Endpoint Arguments
            identifier, technology, scheduler, uri);
            
        auto& [identifier, registration] = *itr;
        if (m_upSecurityMediator) [[likely]] {
            m_upSecurityMediator->BindSecurityContext(registration.GetWritableMessageContext());
        }   
    }

    // When an endpoint registers a connection with this peer, the mediator needs to notify the
    // observers that this peer has been connected to a new endpoint. 
    if (m_pPeerMediator) [[likely]] {
        m_pPeerMediator->DispatchPeerStateChange(
            weak_from_this(), identifier, technology, ConnectionState::Connected);
    }
}

//------------------------------------------------------------------------------------------------

void BryptPeer::WithdrawEndpoint(
    Endpoints::EndpointIdType identifier, Endpoints::TechnologyType technology)
{
    {
        std::scoped_lock lock(m_endpointsMutex);
        m_endpoints.erase(identifier);
    }

    // When an endpoint withdraws its registration from the peer, the mediator needs to notify
    // observer's that peer has been disconnected from that endpoint. 
    if (m_pPeerMediator) [[likely]] {
        m_pPeerMediator->DispatchPeerStateChange(
            weak_from_this(), identifier, technology, ConnectionState::Disconnected);
    }
}

//------------------------------------------------------------------------------------------------

bool BryptPeer::IsActive() const
{
    std::scoped_lock lock(m_endpointsMutex);
    return (m_endpoints.size() != 0);
}

//------------------------------------------------------------------------------------------------

bool CBryptPeer::IsEndpointRegistered(Endpoints::EndpointIdType identifier) const
{
    std::scoped_lock lock(m_endpointsMutex);
    auto const itr = m_endpoints.find(identifier);
    return (itr != m_endpoints.end());
}

//------------------------------------------------------------------------------------------------

std::optional<std::string> BryptPeer::GetRegisteredEntry(
    Endpoints::EndpointIdType identifier) const
{
    std::scoped_lock lock(m_endpointsMutex);
    if (auto const& itr = m_endpoints.find(identifier); itr != m_endpoints.end()) [[likely]] {
        auto const& [key, endpoint] = *itr;
        if (auto const entry = endpoint.GetEntry(); !entry.empty()) {
            return entry;
        }
    }

    return {};
}

//------------------------------------------------------------------------------------------------

std::optional<MessageContext> BryptPeer::GetMessageContext(
    Endpoints::EndpointIdType identifier) const
{
    std::scoped_lock lock(m_endpointsMutex);
    if (auto const& itr = m_endpoints.find(identifier); itr != m_endpoints.end()) [[likely]] {
        auto const& [key, endpoint] = *itr;
        return endpoint.GetMessageContext();
    }

    return {};
}

//------------------------------------------------------------------------------------------------

std::size_t BryptPeer::RegisteredEndpointCount() const
{
    std::scoped_lock lock(m_endpointsMutex);
    return m_endpoints.size();
}

//------------------------------------------------------------------------------------------------

void BryptPeer::AttachSecurityMediator(std::unique_ptr<SecurityMediator>&& upSecurityMediator)
{
    std::scoped_lock lock(m_mediatorMutex);
    m_upSecurityMediator = std::move(upSecurityMediator);  // Take ownership of the mediator.

    if (!m_upSecurityMediator) [[unlikely]] { return; }

    // Ensure any registered endpoints have their message contexts updated to the new mediator's
    // security context.
    {
        std::scoped_lock lock(m_endpointsMutex);
        for (auto& [identifier, registration]: m_endpoints) {
            m_upSecurityMediator->BindSecurityContext(registration.GetWritableMessageContext());
        }
    }

    // Bind ourselves to the mediator in order to allow it to manage our security state. 
    // The mediator will control our receiver to ensure messages are processed correctly.
    m_upSecurityMediator->BindPeer(shared_from_this());
}

//------------------------------------------------------------------------------------------------

Security::State BryptPeer::GetSecurityState() const
{
    if (!m_upSecurityMediator) [[unlikely]] { return Security::State::Unauthorized; } 
    return m_upSecurityMediator->GetSecurityState();
}

//------------------------------------------------------------------------------------------------

bool BryptPeer::IsFlagged() const
{
    if (!m_upSecurityMediator) [[unlikely]] { return true; }
    return (m_upSecurityMediator->GetSecurityState() == Security::State::Flagged);
}

//------------------------------------------------------------------------------------------------

bool BryptPeer::IsAuthorized() const
{
    if (!m_upSecurityMediator) [[unlikely]] { return false; }
    return (m_upSecurityMediator->GetSecurityState() == Security::State::Authorized);
}

//------------------------------------------------------------------------------------------------
