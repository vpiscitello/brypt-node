//----------------------------------------------------------------------------------------------------------------------
// File: Proxy.cpp
// Description: 
//----------------------------------------------------------------------------------------------------------------------
#include "Proxy.hpp"
#include "BryptIdentifier/BryptIdentifier.hpp"
#include "BryptIdentifier/ReservedIdentifiers.hpp"
#include "BryptMessage/ApplicationMessage.hpp"
#include "BryptMessage/MessageContext.hpp"
#include "BryptNode/ServiceProvider.hpp"
#include "Components/Network/Address.hpp"
#include "Components/Network/ConnectionState.hpp"
#include "Components/Security/SecurityState.hpp"
#include "Components/State/NodeState.hpp"
#include "Interfaces/PeerMediator.hpp"
#include "Interfaces/SecurityStrategy.hpp"
//----------------------------------------------------------------------------------------------------------------------
#include <cassert>
//----------------------------------------------------------------------------------------------------------------------

Peer::Proxy::Proxy(
    InstanceToken, Node::Identifier const& identifier, std::shared_ptr<Node::ServiceProvider> const& spProvider)
    : m_spIdentifier()
    , m_wpNodeState(spProvider->Fetch<NodeState>())
    , m_wpMediator(spProvider->Fetch<IPeerMediator>())
    , m_authorization(Security::State::Unauthorized)
    , m_securityMutex()
    , m_upResolver()
    , m_upSecurityStrategy()
    , m_endpointsMutex()
    , m_endpoints()
    , m_receiverMutex()
    , m_pEnabledProcessor()
    , m_wpCoreProcessor(spProvider->Fetch<IMessageSink>())
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

template<>
Node::SharedIdentifier const& Peer::Proxy::GetIdentifier<Node::SharedIdentifier>() const
{
    return m_spIdentifier;
}

//----------------------------------------------------------------------------------------------------------------------

template<>
Node::Internal::Identifier const& Peer::Proxy::GetIdentifier<Node::Internal::Identifier>() const
{
    assert(m_spIdentifier);
    return static_cast<Node::Internal::Identifier const&>(*m_spIdentifier);
}

//----------------------------------------------------------------------------------------------------------------------

template<>
Node::External::Identifier const& Peer::Proxy::GetIdentifier<Node::External::Identifier>() const
{
    assert(m_spIdentifier);
    return static_cast<Node::External::Identifier const&>(*m_spIdentifier);
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
    std::scoped_lock endpointsLock{ m_endpointsMutex };
    if (auto const itr = m_endpoints.find(identifier); itr != m_endpoints.end()) [[likely]] {
        m_statistics.IncrementReceivedCount();

        auto const& [key, registration] = *itr;
        auto const& context = registration.GetMessageContext();

        // Forward the message through the message sink
        std::scoped_lock sinkLock{ m_receiverMutex };
        if (m_pEnabledProcessor) [[likely]] {
            return m_pEnabledProcessor->CollectMessage(weak_from_this(), context, buffer);
        }
    }

    return false;
}

//----------------------------------------------------------------------------------------------------------------------

bool Peer::Proxy::ScheduleReceive(Network::Endpoint::Identifier identifier, std::span<std::uint8_t const> buffer)
{
    std::scoped_lock endpointsLock{ m_endpointsMutex };
    if (auto const itr = m_endpoints.find(identifier); itr != m_endpoints.end()) [[likely]] {
        m_statistics.IncrementReceivedCount();

        auto const& [key, registration] = *itr;
        auto const& context = registration.GetMessageContext();

        // Forward the message through the message sink
        std::scoped_lock sinkLock{ m_receiverMutex };
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

    std::scoped_lock endpointLock{ m_endpointsMutex };
    if (auto const itr = m_endpoints.find(identifier); itr != m_endpoints.end()) [[likely]] {
        m_statistics.IncrementSentCount();

        auto const& [key, endpoint] = *itr;
        auto const& scheduler = endpoint.GetMessageAction();
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

    std::scoped_lock endpointLock{ m_endpointsMutex };
    if (auto const itr = m_endpoints.find(identifier); itr != m_endpoints.end()) [[likely]] {
        m_statistics.IncrementSentCount();

        auto const& [key, endpoint] = *itr;
        auto const& scheduler = endpoint.GetMessageAction();
        assert(m_spIdentifier && scheduler);
        return scheduler(*m_spIdentifier, Network::MessageVariant{spSharedPack});
    }

    return false;
}

//----------------------------------------------------------------------------------------------------------------------

void Peer::Proxy::RegisterEndpoint(Registration const& registration)
{
    {
        std::scoped_lock lock{ m_endpointsMutex };
        auto [itr, result] = m_endpoints.try_emplace(registration.GetEndpointIdentifier(), registration);
        BindSecurityContext(itr->second.GetWritableMessageContext());
    }

    // If this peer has already been marked as authorized, then dispatch the new address. Otherwise, the notification
    // is deferred until an exchange is successfully completed. 
    if (m_authorization == Security::State::Authorized) {
        auto const spMediator = m_wpMediator.lock();
        assert(spMediator);
        auto const& identifier = registration.GetEndpointIdentifier();
        auto const& address = registration.GetAddress();
        spMediator->OnEndpointRegistered(shared_from_this(), identifier, address);
    }
}

//----------------------------------------------------------------------------------------------------------------------

void Peer::Proxy::RegisterEndpoint(
    Network::Endpoint::Identifier identifier,
    Network::Protocol protocol,
    Network::RemoteAddress const& address,
    Network::MessageAction const& scheduler,
    Network::DisconnectAction const& disconnector)
{
    {
        std::scoped_lock lock{ m_endpointsMutex };
        auto [itr, result] = m_endpoints.try_emplace(
            // Registered Endpoint Key
            identifier,
            // Registered Endpoint Arguments
            identifier, protocol, address, scheduler, disconnector);
        BindSecurityContext(itr->second.GetWritableMessageContext());
    }

    // If this peer has already been marked as authorized, then dispatch the new address. Otherwise, the notification
    // is deferred until an exchange is successfully completed. 
    if (m_authorization == Security::State::Authorized) {
        auto const spMediator = m_wpMediator.lock();
        assert(spMediator);
        spMediator->OnEndpointRegistered(shared_from_this(), identifier, address);
    }
}

//----------------------------------------------------------------------------------------------------------------------

void Peer::Proxy::WithdrawEndpoint(Network::Endpoint::Identifier identifier, WithdrawalCause cause)
{
    bool reset = false;
    RegisteredEndpoints::node_type extracted;
    {
        std::scoped_lock lock{ m_endpointsMutex };
        if (extracted = m_endpoints.extract(identifier); !extracted) { return; }
        reset = m_endpoints.empty();
    }

    auto const spMediator = m_wpMediator.lock();
    auto const& address = extracted.mapped().GetAddress();
    assert(spMediator);
    spMediator->OnEndpointWithdrawn(shared_from_this(), identifier, address, cause);

    // If this was the last registered endpoint for the peer, unset the authorization state and enabled processor
    // if this peer reconnects another exchange will need to be conducted as nodes do not save keys to the disk. 
    if (reset) {
        // If there is a resolver, cancel the operation and the resolver will manage setting the authorization and 
        // enabled processor. Otherwise, we need to unset the values directly. 
        if (m_upResolver) { 
            m_upResolver.reset();
            return;
        }
        
        std::scoped_lock lock{ m_receiverMutex };
        m_authorization = Security::State::Unauthorized;
        m_pEnabledProcessor = nullptr;
    }
}

//----------------------------------------------------------------------------------------------------------------------

bool Peer::Proxy::IsActive() const
{
    std::scoped_lock lock{ m_endpointsMutex };
    return (m_endpoints.size() != 0);
}

//----------------------------------------------------------------------------------------------------------------------

bool Peer::Proxy::IsEndpointRegistered(Network::Endpoint::Identifier identifier) const
{
    std::scoped_lock lock{ m_endpointsMutex };
    return (m_endpoints.find(identifier) != m_endpoints.end());
}

//----------------------------------------------------------------------------------------------------------------------

bool Peer::Proxy::IsEndpointRegistered(Network::Address const& address) const
{
    std::scoped_lock lock{ m_endpointsMutex };
    return std::ranges::any_of(m_endpoints, [&address] (auto const& entry) {
        return address == entry.second.GetAddress();
    });
}

//----------------------------------------------------------------------------------------------------------------------

std::optional<Message::Context> Peer::Proxy::GetMessageContext(Network::Endpoint::Identifier identifier) const
{
    std::scoped_lock lock{ m_endpointsMutex };
    if (auto const& itr = m_endpoints.find(identifier); itr != m_endpoints.end()) [[likely]] {
        auto const& [key, endpoint] = *itr;
        return endpoint.GetMessageContext();
    }

    return {};
}

//----------------------------------------------------------------------------------------------------------------------

std::optional<Network::RemoteAddress> Peer::Proxy::GetRegisteredAddress(Network::Endpoint::Identifier identifier) const
{
    std::scoped_lock lock{ m_endpointsMutex };
    if (auto const& itr = m_endpoints.find(identifier); itr != m_endpoints.end()) [[likely]] {
        return itr->second.GetAddress();
    }
    return {};
}

//----------------------------------------------------------------------------------------------------------------------

std::size_t Peer::Proxy::RegisteredEndpointCount() const
{
    std::scoped_lock lock{ m_endpointsMutex };
    return m_endpoints.size();
}

//----------------------------------------------------------------------------------------------------------------------

bool Peer::Proxy::ScheduleDisconnect() const
{
    std::scoped_lock lock{ m_endpointsMutex };
    for (auto const& [identifier, registration] : m_endpoints) {
        auto const& disconnect = registration.GetDisconnectAction();
        assert(disconnect);
        disconnect(registration.GetAddress());
    }
    return !m_endpoints.empty(); // Currently, the return value indicates if the proxy had endpoints to disconnect. 
}

//----------------------------------------------------------------------------------------------------------------------

bool Peer::Proxy::AttachResolver(std::unique_ptr<Resolver>&& upResolver)
{
    std::scoped_lock lock{ m_securityMutex, m_receiverMutex };
    if (m_upResolver || !upResolver) [[unlikely]] { return false; }
    m_upResolver = std::move(upResolver);
    
    auto* const pExchangeSink = m_upResolver->GetExchangeSink();
    if (!pExchangeSink) [[unlikely]] { return false; }

    m_pEnabledProcessor = pExchangeSink;

    m_upResolver->BindCompletionHandlers(
        [this] (std::unique_ptr<ISecurityStrategy>&& upSecurityStrategy) {
            std::scoped_lock lock{ m_securityMutex };
            m_upSecurityStrategy = std::move(upSecurityStrategy);
        },
        [this] (ExchangeStatus status) {
            std::scoped_lock lock{ m_securityMutex, m_receiverMutex };

            // Set the security state and enabled processor to the default state. If this is a success notification,
            // they will be set accordingly. 
            m_authorization = Security::State::Unauthorized; // Unset the authorization state. 
            m_pEnabledProcessor = nullptr; // Unset the processor such that messages are dropped.

            if (status == ExchangeStatus::Success) {
                m_authorization = Security::State::Authorized; // The peer is authorized and allowed into the core. 
                // If we can obtain the core's message processor (i.e. not in shutdown state), set the enabled processor
                // such that received messages are forwarded into the core. 
                if (auto const spProcessor = m_wpCoreProcessor.lock(); spProcessor) [[likely]] {
                    m_pEnabledProcessor = spProcessor.get();
                }

                // For each registered endpoint, dispatch the associated address to notify the core of the newly
                // connected addresses. 
                for (auto const& [identifier, registration] : m_endpoints) {
                    auto const spMediator = m_wpMediator.lock();
                    auto const& address = registration.GetAddress();
                    assert(spMediator);
                    spMediator->OnEndpointRegistered(shared_from_this(), identifier, address);
                }
            }

            m_upResolver.reset(); // The resolver (i.e. the temporary exchange message processor) is no longer needed. 
        });

    return true; 
}

//----------------------------------------------------------------------------------------------------------------------

bool Peer::Proxy::StartExchange(
    Node::SharedIdentifier const& spSource,
    Security::Strategy strategy,
    Security::Role role,
    std::shared_ptr<IConnectProtocol> const& spProtocol)
{
    assert(*spSource != *m_spIdentifier);

    {
        std::shared_lock lock{ m_securityMutex };
        if (m_upResolver) [[unlikely]] { return false; }
    }

    auto upResolver = std::make_unique<Resolver>(spSource, Security::Context::Unique);
    switch (role) {
        case Security::Role::Acceptor: {
            bool const result = upResolver->SetupExchangeAcceptor(strategy);
            return result && AttachResolver(std::move(upResolver));
        }
        case Security::Role::Initiator: {
            assert(false); // Currently, we only support starting an accepting resolver. 
            auto optRequest = upResolver->SetupExchangeInitiator(strategy, spProtocol);
            if (!optRequest || !AttachResolver(std::move(upResolver))) { return false; }
            return ScheduleSend(std::numeric_limits<std::uint32_t>::max(), std::move(*optRequest));
        }
        default: assert(false); return false; // What is this?
    }

    return false; // How did we fallthrough to here? 
}

//----------------------------------------------------------------------------------------------------------------------

Security::State Peer::Proxy::GetAuthorization() const
{
    std::shared_lock lock{ m_securityMutex };
    return m_authorization;
}

//----------------------------------------------------------------------------------------------------------------------

bool Peer::Proxy::IsFlagged() const
{
    std::shared_lock lock{ m_securityMutex };
    return m_authorization == Security::State::Flagged;
}

//----------------------------------------------------------------------------------------------------------------------

bool Peer::Proxy::IsAuthorized() const
{
    std::shared_lock lock{ m_securityMutex };
    return m_authorization == Security::State::Authorized;
}

//----------------------------------------------------------------------------------------------------------------------

void Peer::Proxy::BindSecurityContext(Message::Context& context) const
{
    auto const& upStrategy = m_upSecurityStrategy;

    context.BindEncryptionHandlers(
        [&mutex = m_securityMutex, &upStrategy] (auto buffer, auto nonce) -> Security::Encryptor::result_type {
            std::shared_lock lock{ mutex };
            if (!upStrategy) [[unlikely]] { return {}; }
            return upStrategy->Encrypt(buffer, nonce);
        },
        [&mutex = m_securityMutex, &upStrategy] (auto buffer, auto nonce) -> Security::Decryptor::result_type {
            std::shared_lock lock{ mutex };
            if (!upStrategy) [[unlikely]] { return {}; }
            return upStrategy->Decrypt(buffer, nonce);
        });

    context.BindSignatureHandlers(
        [&mutex = m_securityMutex, &upStrategy] (auto& buffer) -> Security::Signator::result_type {
            std::shared_lock lock{ mutex };
            if (!upStrategy) [[unlikely]] { return -1; }
            return upStrategy->Sign(buffer);
        },
        [&mutex = m_securityMutex, &upStrategy] (auto buffer) -> Security::Verifier::result_type {
            std::shared_lock lock{ mutex };
            if (!upStrategy) [[unlikely]] { return Security::VerificationStatus::Failed; }
            return upStrategy->Verify(buffer);
        },
        [&mutex = m_securityMutex, &upStrategy] () -> Security::SignatureSizeGetter::result_type {
            std::shared_lock lock{ mutex };
            if (!upStrategy) [[unlikely]] { return 0; }
            return upStrategy->GetSignatureSize();
        });
}

//----------------------------------------------------------------------------------------------------------------------

template<>
void Peer::Proxy::SetMediator<InvokeContext::Test>(std::weak_ptr<IPeerMediator> const& wpMediator)
{
    m_wpMediator = wpMediator;
}

//----------------------------------------------------------------------------------------------------------------------

template<>
void Peer::Proxy::SetReceiver<InvokeContext::Test>(IMessageSink* const pMessageSink)
{
    std::scoped_lock lock{ m_receiverMutex };
    m_pEnabledProcessor = pMessageSink;
}

//----------------------------------------------------------------------------------------------------------------------

template<>
void Peer::Proxy::SetAuthorization<InvokeContext::Test>(Security::State state)
{
    m_authorization = state;
}

//----------------------------------------------------------------------------------------------------------------------

template <>
void Peer::Proxy::AttachSecurityStrategy<InvokeContext::Test>(std::unique_ptr<ISecurityStrategy>&& upStrategy)
{
    assert(!m_upResolver);
    m_upSecurityStrategy = std::move(upStrategy);
    assert(m_upSecurityStrategy);

    // Ensure any registered endpoints have their message contexts updated to the new mediator's security context.
    std::scoped_lock endpointsLock{ m_endpointsMutex };
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
    std::scoped_lock lock{ m_endpointsMutex };
    auto [itr, result] = m_endpoints.try_emplace(registration.GetEndpointIdentifier(), registration);
    BindSecurityContext(itr->second.GetWritableMessageContext());
}

//----------------------------------------------------------------------------------------------------------------------

template <>
void Peer::Proxy::RegisterSilentEndpoint<InvokeContext::Test>(
    Network::Endpoint::Identifier identifier,
    Network::Protocol protocol,
    Network::RemoteAddress const& address,
    Network::MessageAction const& scheduler)
{
    std::scoped_lock lock{ m_endpointsMutex };
    auto [itr, result] =  m_endpoints.try_emplace(identifier, identifier, protocol, address, scheduler);
    BindSecurityContext(itr->second.GetWritableMessageContext());
}

//----------------------------------------------------------------------------------------------------------------------

template <>
void Peer::Proxy::WithdrawSilentEndpoint<InvokeContext::Test>(
    Network::Endpoint::Identifier identifier, [[maybe_unused]] Network::Protocol protocol)
{
    std::scoped_lock lock{ m_endpointsMutex };
    m_endpoints.erase(identifier);
}

//----------------------------------------------------------------------------------------------------------------------
