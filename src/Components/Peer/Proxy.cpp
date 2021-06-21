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
#include "Components/Security/SecurityState.hpp"
#include "Interfaces/PeerMediator.hpp"
#include "Interfaces/SecurityStrategy.hpp"
//----------------------------------------------------------------------------------------------------------------------
#include <cassert>
//----------------------------------------------------------------------------------------------------------------------

Peer::Proxy::Proxy(
    Node::Identifier const& identifier,
    IPeerMediator* const pPeerMediator,
    std::weak_ptr<IMessageSink> const& wpAuthorizedProcessor)
    : m_spIdentifier()
    , m_pPeerMediator(pPeerMediator)
    , m_securityMutex()
    , m_state(Security::State::Unauthorized)
    , m_upResolver()
    , m_upSecurityStrategy()
    , m_endpointsMutex()
    , m_endpoints()
    , m_receiverMutex()
    , m_pEnabledProcessor()
    , m_wpAuthorizedProcessor(wpAuthorizedProcessor)
    , m_statistics()
{
    // We must always be constructed with an identifier that can uniquely identify the peer. 
    assert(identifier.IsValid() && !Node::IsIdentifierReserved(identifier));
    m_spIdentifier = std::make_shared<Node::Identifier const>(identifier);
}

//----------------------------------------------------------------------------------------------------------------------

Peer::Proxy::~Proxy()
{
    // If a resolver exists, it must be given a chance to fire the completion handlers before we destroy our resources.  
    m_upResolver.reset();
}

//----------------------------------------------------------------------------------------------------------------------

Node::SharedIdentifier Peer::Proxy::GetNodeIdentifier() const
{
    return m_spIdentifier;
}

//----------------------------------------------------------------------------------------------------------------------

Node::Internal::Identifier::Type Peer::Proxy::GetInternalIdentifier() const
{
    assert(m_spIdentifier);
    return m_spIdentifier->GetInternalValue();
}

//----------------------------------------------------------------------------------------------------------------------

std::uint32_t Peer::Proxy::GetSentCount() const
{
    return m_statistics.GetSentCount();
}

//----------------------------------------------------------------------------------------------------------------------

std::uint32_t Peer::Proxy::GetReceivedCount() const
{
    return m_statistics.GetReceivedCount();
}

//----------------------------------------------------------------------------------------------------------------------

bool Peer::Proxy::ScheduleReceive(Network::Endpoint::Identifier identifier, std::string_view buffer)
{
    std::scoped_lock endpointsLock(m_endpointsMutex);
    if (auto const itr = m_endpoints.find(identifier); itr != m_endpoints.end()) [[likely]] {
        m_statistics.IncrementReceivedCount();

        auto const& [key, registration] = *itr;
        auto const& context = registration.GetMessageContext();

        // Forward the message through the message sink
        std::scoped_lock sinkLock(m_receiverMutex);
        if (m_pEnabledProcessor) [[likely]] {
            return m_pEnabledProcessor->CollectMessage(weak_from_this(), context, buffer);
        }
    }

    return false;
}

//----------------------------------------------------------------------------------------------------------------------

bool Peer::Proxy::ScheduleReceive(Network::Endpoint::Identifier identifier, std::span<std::uint8_t const> buffer)
{
    std::scoped_lock endpointsLock(m_endpointsMutex);
    if (auto const itr = m_endpoints.find(identifier); itr != m_endpoints.end()) [[likely]] {
        m_statistics.IncrementReceivedCount();

        auto const& [key, registration] = *itr;
        auto const& context = registration.GetMessageContext();

        // Forward the message through the message sink
        std::scoped_lock sinkLock(m_receiverMutex);
        if (m_pEnabledProcessor) [[likely]] {
            return m_pEnabledProcessor->CollectMessage(weak_from_this(), context, buffer);
        }
    }

    return false;
}

//----------------------------------------------------------------------------------------------------------------------

bool Peer::Proxy::ScheduleSend(Network::Endpoint::Identifier identifier, std::string&& message) const
{
    assert(!message.empty());

    std::scoped_lock endpointLock(m_endpointsMutex);
    if (auto const itr = m_endpoints.find(identifier); itr != m_endpoints.end()) [[likely]] {
        m_statistics.IncrementSentCount();

        auto const& [key, endpoint] = *itr;
        auto const& scheduler = endpoint.GetScheduler();
        assert(m_spIdentifier && scheduler);
        return scheduler(*m_spIdentifier, Network::MessageVariant{std::move(message)});
    }

    return false;
}

//----------------------------------------------------------------------------------------------------------------------

bool Peer::Proxy::ScheduleSend(
    Network::Endpoint::Identifier identifier, Message::ShareablePack const& spSharedPack) const
{
    assert(spSharedPack && !spSharedPack->empty());

    std::scoped_lock endpointLock(m_endpointsMutex);
    if (auto const itr = m_endpoints.find(identifier); itr != m_endpoints.end()) [[likely]] {
        m_statistics.IncrementSentCount();

        auto const& [key, endpoint] = *itr;
        auto const& scheduler = endpoint.GetScheduler();
        assert(m_spIdentifier && scheduler);
        return scheduler(*m_spIdentifier, Network::MessageVariant{spSharedPack});
    }

    return false;
}

//----------------------------------------------------------------------------------------------------------------------

void Peer::Proxy::RegisterEndpoint(Registration const& registration)
{
    {
        std::scoped_lock lock(m_endpointsMutex);
        auto [itr, result] = m_endpoints.try_emplace(registration.GetEndpointIdentifier(), registration);
        BindSecurityContext(itr->second.GetWritableMessageContext());
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
    Network::MessageScheduler const& scheduler)
{
    {
        std::scoped_lock lock(m_endpointsMutex);
        auto [itr, result] = m_endpoints.try_emplace(
            // Registered Endpoint Key
            identifier,
            // Registered Endpoint Arguments
            identifier, protocol, address, scheduler);
        BindSecurityContext(itr->second.GetWritableMessageContext());
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
    return (m_endpoints.find(identifier) != m_endpoints.end());
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
        if (auto const address = endpoint.GetAddress(); address.IsBootstrapable()) { return address; }
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

bool Peer::Proxy::AttachResolver(std::unique_ptr<Resolver>&& upResolver)
{
    std::scoped_lock lock(m_securityMutex, m_receiverMutex);
    if (m_upResolver || !upResolver) [[unlikely]] { return false; }
    m_upResolver = std::move(upResolver);
    
    auto* const pExchangeSink = m_upResolver->GetExchangeSink();
    if (!pExchangeSink) [[unlikely]] { return false; }

    m_pEnabledProcessor = pExchangeSink;

    m_upResolver->BindCompletionHandlers(
        [this] (std::unique_ptr<ISecurityStrategy>&& upSecurityStrategy)
        {
            std::scoped_lock lock(m_securityMutex);
            m_upSecurityStrategy = std::move(upSecurityStrategy);
        },
        [this] (ExchangeStatus status)
        {
            std::scoped_lock lock(m_securityMutex, m_receiverMutex);

            // If we have been notified us of a failed exchange unset the message sink for the peer 
            // and mark the peer as unauthorized. 
            m_state = Security::State::Unauthorized;
            m_pEnabledProcessor = nullptr;
            
            // If we have been notified us of a successful exchange set the message sink for the peer 
            // to the authorized sink and mark the peer as authorized. 
            if (status == ExchangeStatus::Success) {
                m_state = Security::State::Authorized;
                if (auto const spProcessor = m_wpAuthorizedProcessor.lock(); spProcessor) [[likely]] {
                    m_pEnabledProcessor = spProcessor.get();
                }
            }

            m_upResolver.reset();
        });

    return true; 
}

//----------------------------------------------------------------------------------------------------------------------

bool Peer::Proxy::StartExchange(
    Security::Strategy strategy, Security::Role role, std::shared_ptr<IConnectProtocol> const& spProtocol)
{
    if (m_upResolver) [[unlikely]] { return false; }
    auto upResolver = std::make_unique<Resolver>(m_spIdentifier, Security::Context::Unique);
    switch (role) {
        case Security::Role::Acceptor: {
            bool const result = upResolver->SetupExchangeAcceptor(strategy);
            return result && AttachResolver(std::move(upResolver));
        }
        case Security::Role::Initiator: {
            assert(false); // Currently, we only accept starting an accepting resolver. 
            auto optRequest = upResolver->SetupExchangeInitiator(strategy, spProtocol);
            if (!optRequest || !AttachResolver(std::move(upResolver))) { return false; }
            return ScheduleSend(std::numeric_limits<std::uint32_t>::max(), std::move(*optRequest));
        }
        default: assert(false); return false; // What is this?
    }
    assert(false); return false; // How did we fallthrough to here? 
}

//----------------------------------------------------------------------------------------------------------------------

Security::State Peer::Proxy::GetSecurityState() const
{
    std::shared_lock lock(m_securityMutex);
    return m_state;
}

//----------------------------------------------------------------------------------------------------------------------

bool Peer::Proxy::IsFlagged() const
{
    std::shared_lock lock(m_securityMutex);
    return m_state == Security::State::Flagged;
}

//----------------------------------------------------------------------------------------------------------------------

bool Peer::Proxy::IsAuthorized() const
{
    std::shared_lock lock(m_securityMutex);
    return m_state == Security::State::Authorized;
}

//----------------------------------------------------------------------------------------------------------------------

void Peer::Proxy::BindSecurityContext(MessageContext& context) const
{
    auto const& upStrategy = m_upSecurityStrategy;

    context.BindEncryptionHandlers(
        [&mutex = m_securityMutex, &upStrategy] (auto buffer, auto nonce) -> Security::Encryptor::result_type
        {
            std::shared_lock lock(mutex);
            if (!upStrategy) [[unlikely]] { return {}; }
            return upStrategy->Encrypt(buffer, nonce);
        },
        [&mutex = m_securityMutex, &upStrategy] (auto buffer, auto nonce) -> Security::Decryptor::result_type
        {
            std::shared_lock lock(mutex);
            if (!upStrategy) [[unlikely]] { return {}; }
            return upStrategy->Decrypt(buffer, nonce);
        });

    context.BindSignatureHandlers(
        [&mutex = m_securityMutex, &upStrategy] (auto& buffer) -> Security::Signator::result_type
        {
            std::shared_lock lock(mutex);
            if (!upStrategy) [[unlikely]] { return -1; }
            return upStrategy->Sign(buffer);
        },
        [&mutex = m_securityMutex, &upStrategy] (auto buffer) -> Security::Verifier::result_type
        {
            std::shared_lock lock(mutex);
            if (!upStrategy) [[unlikely]] { return Security::VerificationStatus::Failed; }
            return upStrategy->Verify(buffer);
        },
        [&mutex = m_securityMutex, &upStrategy] () -> Security::SignatureSizeGetter::result_type
        {
            std::shared_lock lock(mutex);
            if (!upStrategy) [[unlikely]] { return 0; }
            return upStrategy->GetSignatureSize();
        });
}

//----------------------------------------------------------------------------------------------------------------------

template<>
void Peer::Proxy::SetReceiver<InvokeContext::Test>(IMessageSink* const pMessageSink)
{
    std::scoped_lock lock(m_receiverMutex);
    m_pEnabledProcessor = pMessageSink;
}

//----------------------------------------------------------------------------------------------------------------------

template <>
void Peer::Proxy::AttachSecurityStrategy<InvokeContext::Test>(std::unique_ptr<ISecurityStrategy>&& upStrategy)
{
    assert(!m_upResolver);
    m_upSecurityStrategy = std::move(upStrategy);
    assert(m_upSecurityStrategy);

    // Ensure any registered endpoints have their message contexts updated to the new mediator's security context.
    std::scoped_lock endpointsLock(m_endpointsMutex);
    for (auto& [identifier, registration]: m_endpoints) {
        BindSecurityContext(registration.GetWritableMessageContext());
    }
}

//----------------------------------------------------------------------------------------------------------------------

template <>
void Peer::Proxy::DetachResolver<InvokeContext::Test>()
{
    m_upResolver.reset();
}

//----------------------------------------------------------------------------------------------------------------------

template <>
void Peer::Proxy::RegisterSilentEndpoint<InvokeContext::Test>(Registration const& registration)
{
    std::scoped_lock lock(m_endpointsMutex);
    auto [itr, result] = m_endpoints.try_emplace(registration.GetEndpointIdentifier(), registration);
    BindSecurityContext(itr->second.GetWritableMessageContext());
}

//----------------------------------------------------------------------------------------------------------------------

template <>
void Peer::Proxy::RegisterSilentEndpoint<InvokeContext::Test>(
    Network::Endpoint::Identifier identifier,
    Network::Protocol protocol,
    Network::RemoteAddress const& address,
    Network::MessageScheduler const& scheduler)
{
    std::scoped_lock lock(m_endpointsMutex);
    auto [itr, result] =  m_endpoints.try_emplace(identifier, identifier, protocol, address, scheduler);
    BindSecurityContext(itr->second.GetWritableMessageContext());
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
