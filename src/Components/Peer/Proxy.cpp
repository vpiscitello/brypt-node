//----------------------------------------------------------------------------------------------------------------------
// File: Proxy.cpp
// Description: 
//----------------------------------------------------------------------------------------------------------------------
#include "Proxy.hpp"
#include "BryptIdentifier/BryptIdentifier.hpp"
#include "BryptIdentifier/ReservedIdentifiers.hpp"
#include "BryptMessage/ApplicationMessage.hpp"
#include "BryptMessage/MessageContext.hpp"
#include "Components/Network/Address.hpp"
#include "Components/Security/Mediator.hpp"
#include "Components/Security/SecurityState.hpp"
#include "Interfaces/PeerMediator.hpp"
//----------------------------------------------------------------------------------------------------------------------
#include <cassert>
//----------------------------------------------------------------------------------------------------------------------

Peer::Proxy::Proxy(Node::Identifier const& identifier, IPeerMediator* const pPeerMediator)
    : m_pPeerMediator(pPeerMediator)
    , m_dataMutex()
    , m_spNodeIdentifier()
    , m_statistics()
    , m_mediatorMutex()
    , m_upSecurityMediator()
    , m_endpointsMutex()
    , m_endpoints()
    , m_receiverMutex()
    , m_pMessageSink()
{
    // We must always be constructed with an identifier that can uniquely identify the peer. 
    assert(identifier.IsValid() && !Node::IsIdentifierReserved(identifier));
    m_spNodeIdentifier = std::make_shared<Node::Identifier const>(identifier);
}

//----------------------------------------------------------------------------------------------------------------------

Peer::Proxy::~Proxy()
{
}

//----------------------------------------------------------------------------------------------------------------------

Node::SharedIdentifier Peer::Proxy::GetNodeIdentifier() const
{
    std::scoped_lock lock(m_dataMutex);
    return m_spNodeIdentifier;
}

//----------------------------------------------------------------------------------------------------------------------

Node::Internal::Identifier::Type Peer::Proxy::GetInternalIdentifier() const
{
    std::scoped_lock lock(m_dataMutex);
    assert(m_spNodeIdentifier);
    return m_spNodeIdentifier->GetInternalValue();
}

//----------------------------------------------------------------------------------------------------------------------

std::uint32_t Peer::Proxy::GetSentCount() const
{
    std::scoped_lock lock(m_dataMutex);
    return m_statistics.GetSentCount();
}

//----------------------------------------------------------------------------------------------------------------------

std::uint32_t Peer::Proxy::GetReceivedCount() const
{
    std::scoped_lock lock(m_dataMutex);
    return m_statistics.GetReceivedCount();
}

//----------------------------------------------------------------------------------------------------------------------

void Peer::Proxy::SetReceiver(IMessageSink* const pMessageSink)
{
    std::scoped_lock lock(m_receiverMutex);
    m_pMessageSink = pMessageSink;
}

//----------------------------------------------------------------------------------------------------------------------

bool Peer::Proxy::ScheduleReceive(Network::Endpoint::Identifier identifier, std::string_view buffer)
{
    {
        std::scoped_lock lock(m_dataMutex);
        m_statistics.IncrementReceivedCount();
    }

    std::scoped_lock endpointsLock(m_endpointsMutex);
    if (auto const itr = m_endpoints.find(identifier); itr != m_endpoints.end()) [[likely]] {
        auto const& [key, registration] = *itr;
        auto const& context = registration.GetMessageContext();

        // Forward the message through the message sink
        std::scoped_lock sinkLock(m_receiverMutex);
        if (m_pMessageSink) [[likely]] {
            return m_pMessageSink->CollectMessage(weak_from_this(), context, buffer);
        }
    }

    return false;
}

//----------------------------------------------------------------------------------------------------------------------

bool Peer::Proxy::ScheduleReceive(Network::Endpoint::Identifier identifier, std::span<std::uint8_t const> buffer)
{
    {
        std::scoped_lock lock(m_dataMutex);
        m_statistics.IncrementReceivedCount();
    }

    std::scoped_lock endpointsLock(m_endpointsMutex);
    if (auto const itr = m_endpoints.find(identifier); itr != m_endpoints.end()) [[likely]] {
        auto const& [key, registration] = *itr;
        auto const& context = registration.GetMessageContext();

        // Forward the message through the message sink
        std::scoped_lock sinkLock(m_receiverMutex);
        if (m_pMessageSink) [[likely]] {
            return m_pMessageSink->CollectMessage(weak_from_this(), context, buffer);
        }
    }

    return false;
}

//----------------------------------------------------------------------------------------------------------------------

bool Peer::Proxy::ScheduleSend(Network::Endpoint::Identifier identifier, std::string_view message) const
{
    {
        std::scoped_lock lock(m_dataMutex);
        m_statistics.IncrementSentCount();
    }

    std::scoped_lock lock(m_endpointsMutex);
    if (auto const itr = m_endpoints.find(identifier); itr != m_endpoints.end()) [[likely]] {
        auto const& [key, endpoint] = *itr;
        auto const& scheduler = endpoint.GetScheduler();
        assert(m_spNodeIdentifier && scheduler);
        return scheduler(*m_spNodeIdentifier, message);
    }

    return false;
}

//----------------------------------------------------------------------------------------------------------------------

void Peer::Proxy::RegisterEndpoint(Registration const& registration)
{
    {
        std::scoped_lock lock(m_endpointsMutex);
        auto [itr, result] = m_endpoints.try_emplace(registration.GetEndpointIdentifier(), registration);

        assert(m_upSecurityMediator);
        m_upSecurityMediator->BindSecurityContext(itr->second.GetWritableMessageContext());
    }

    // When an endpoint registers a connection with this peer, the mediator needs to notify the observers that this 
    // peer has been connected to a new endpoint. 
    assert(m_pPeerMediator);
    m_pPeerMediator->DispatchPeerStateChange(
        weak_from_this(),
        registration.GetEndpointIdentifier(), registration.GetEndpointProtocol(), ConnectionState::Connected);
}

//----------------------------------------------------------------------------------------------------------------------

void Peer::Proxy::RegisterEndpoint(
    Network::Endpoint::Identifier identifier,
    Network::Protocol protocol,
    Network::RemoteAddress const& address,
    MessageScheduler const& scheduler)
{
    {
        std::scoped_lock lock(m_endpointsMutex);
        auto [itr, result] = m_endpoints.try_emplace(
            // Registered Endpoint Key
            identifier,
            // Registered Endpoint Arguments
            identifier, protocol, address, scheduler);
            
        auto& [identifier, registration] = *itr;
        assert(m_upSecurityMediator);
        m_upSecurityMediator->BindSecurityContext(registration.GetWritableMessageContext());
    }

    // When an endpoint registers a connection with this peer, the mediator needs to notify the observers that this 
    // peer has been connected to a new endpoint. 
    assert(m_pPeerMediator);
    m_pPeerMediator->DispatchPeerStateChange(weak_from_this(), identifier, protocol, ConnectionState::Connected);
}

//----------------------------------------------------------------------------------------------------------------------

void Peer::Proxy::WithdrawEndpoint(Network::Endpoint::Identifier identifier, Network::Protocol protocol)
{
    {
        std::scoped_lock lock(m_endpointsMutex);
        m_endpoints.erase(identifier);
    }

    // When an endpoint withdraws its registration from the peer, the mediator needs to notify observer's that peer 
    // has been disconnected from that endpoint. 
    assert(m_pPeerMediator);
    m_pPeerMediator->DispatchPeerStateChange(weak_from_this(), identifier, protocol, ConnectionState::Disconnected);
}

//----------------------------------------------------------------------------------------------------------------------

bool Peer::Proxy::IsActive() const
{
    std::scoped_lock lock(m_endpointsMutex);
    return (m_endpoints.size() != 0);
}

//----------------------------------------------------------------------------------------------------------------------

bool Peer::Proxy::IsEndpointRegistered(Network::Endpoint::Identifier identifier) const
{
    std::scoped_lock lock(m_endpointsMutex);
    auto const itr = m_endpoints.find(identifier);
    return (itr != m_endpoints.end());
}

//----------------------------------------------------------------------------------------------------------------------

std::optional<MessageContext> Peer::Proxy::GetMessageContext(Network::Endpoint::Identifier identifier) const
{
    std::scoped_lock lock(m_endpointsMutex);
    if (auto const& itr = m_endpoints.find(identifier); itr != m_endpoints.end()) [[likely]] {
        auto const& [key, endpoint] = *itr;
        return endpoint.GetMessageContext();
    }

    return {};
}

//----------------------------------------------------------------------------------------------------------------------

std::optional<Network::RemoteAddress> Peer::Proxy::GetRegisteredAddress(Network::Endpoint::Identifier identifier) const
{
    std::scoped_lock lock(m_endpointsMutex);
    if (auto const& itr = m_endpoints.find(identifier); itr != m_endpoints.end()) [[likely]] {
        auto const& [key, endpoint] = *itr;
        if (auto const address = endpoint.GetAddress(); address.IsBootstrapable()) {
            return address;
        }
    }

    return {};
}

//----------------------------------------------------------------------------------------------------------------------

std::size_t Peer::Proxy::RegisteredEndpointCount() const
{
    std::scoped_lock lock(m_endpointsMutex);
    return m_endpoints.size();
}

//----------------------------------------------------------------------------------------------------------------------

void Peer::Proxy::AttachSecurityMediator(std::unique_ptr<Security::Mediator>&& upSecurityMediator)
{
    std::scoped_lock mediatorLock(m_mediatorMutex);
    m_upSecurityMediator = std::move(upSecurityMediator);  // Take ownership of the mediator.
    assert(m_upSecurityMediator);

    // Ensure any registered endpoints have their message contexts updated to the new mediator's
    // security context.
    {
        std::scoped_lock endpointsLock(m_endpointsMutex);
        for (auto& [identifier, registration]: m_endpoints) {
            m_upSecurityMediator->BindSecurityContext(registration.GetWritableMessageContext());
        }
    }

    // Bind ourselves to the mediator in order to allow it to manage our security state. 
    // The mediator will control our receiver to ensure messages are processed correctly.
    m_upSecurityMediator->BindPeer(shared_from_this());
}

//----------------------------------------------------------------------------------------------------------------------

Security::State Peer::Proxy::GetSecurityState() const
{
    assert(m_upSecurityMediator);
    return m_upSecurityMediator->GetSecurityState();
}

//----------------------------------------------------------------------------------------------------------------------

bool Peer::Proxy::IsFlagged() const
{
    assert(m_upSecurityMediator);
    return (m_upSecurityMediator->GetSecurityState() == Security::State::Flagged);
}

//----------------------------------------------------------------------------------------------------------------------

bool Peer::Proxy::IsAuthorized() const
{
    assert(m_upSecurityMediator);
    return (m_upSecurityMediator->GetSecurityState() == Security::State::Authorized);
}

//----------------------------------------------------------------------------------------------------------------------

template <>
void Peer::Proxy::RegisterSilentEndpoint<InvokeContext::Test>(Registration const& registration)
{
    std::scoped_lock lock(m_endpointsMutex);
    m_endpoints.try_emplace(registration.GetEndpointIdentifier(), registration);
}

//----------------------------------------------------------------------------------------------------------------------

template <>
void Peer::Proxy::RegisterSilentEndpoint<InvokeContext::Test>(
    Network::Endpoint::Identifier identifier,
    Network::Protocol protocol,
    Network::RemoteAddress const& address,
    MessageScheduler const& scheduler)
{
    std::scoped_lock lock(m_endpointsMutex);
    m_endpoints.try_emplace(identifier, identifier, protocol, address, scheduler);
}

//----------------------------------------------------------------------------------------------------------------------

template <>
void Peer::Proxy::WithdrawSilentEndpoint<InvokeContext::Test>(
    Network::Endpoint::Identifier identifier, [[maybe_unused]] Network::Protocol protocol)
{
    std::scoped_lock lock(m_endpointsMutex);
    m_endpoints.erase(identifier);
}

//----------------------------------------------------------------------------------------------------------------------
