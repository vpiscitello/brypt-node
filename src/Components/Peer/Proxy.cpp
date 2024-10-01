//----------------------------------------------------------------------------------------------------------------------
// File: Proxy.cpp
// Description: 
//----------------------------------------------------------------------------------------------------------------------
#include "Proxy.hpp"
#include "Components/Awaitable/TrackingService.hpp"
#include "Components/Core/ServiceProvider.hpp"
#include "Components/Identifier/BryptIdentifier.hpp"
#include "Components/Identifier/ReservedIdentifiers.hpp"
#include "Components/Message/ApplicationMessage.hpp"
#include "Components/Message/MessageContext.hpp"
#include "Components/Network/Address.hpp"
#include "Components/Network/ConnectionState.hpp"
#include "Components/Security/SecurityState.hpp"
#include "Interfaces/ResolutionService.hpp"
//----------------------------------------------------------------------------------------------------------------------
#include <cassert>
//----------------------------------------------------------------------------------------------------------------------

Peer::Proxy::Proxy(
    InstanceToken, Node::Identifier const& identifier, std::shared_ptr<Node::ServiceProvider> const& spProvider)
    : m_spIdentifier()
    , m_wpResolutionService(spProvider->Fetch<IResolutionService>())
    , m_wpTrackingService(spProvider->Fetch<Awaitable::TrackingService>())
    , m_authorization(Security::State::Unauthorized)
    , m_securityMutex()
    , m_upResolver()
    , m_upCipherPackage()
    , m_endpointsMutex()
    , m_endpoints()
    , m_associatedMutex()
    , m_associated()
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
            return m_pEnabledProcessor->CollectMessage(context, buffer);
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
            return m_pEnabledProcessor->CollectMessage(context, buffer);
        }
    }

    return false;
}

//----------------------------------------------------------------------------------------------------------------------

std::optional<Awaitable::TrackerKey> Peer::Proxy::Request(
    Message::Application::Builder& builder, Action::OnResponse const& onResponse, Action::OnError const& onError)
{
    auto const spTrackingService = m_wpTrackingService.lock();
    if (!spTrackingService) { return {}; }
    
    std::scoped_lock lock{ m_endpointsMutex };
    if (m_endpoints.empty()) { return {}; }
    
    auto const endpoint = GetOrSetPreferredEndpoint(builder); // Fetch the endpoint to be used to send out the request. 
    if (endpoint == m_endpoints.end()) { return {}; }

    builder.SetDestination(*m_spIdentifier); // Set the destination as the the one represented by this proxy. 

    // Use the tracking service to stage the outgoing request such that the associated callbacks can be 
    // executed when it has been fulfilled. 
    auto const optTrackerKey = spTrackingService->StageRequest(weak_from_this(), onResponse, onError, builder);
    if (!optTrackerKey) { return {}; }

    if (auto const optRequest = builder.ValidatedBuild(); optRequest) [[likely]] {
        m_statistics.IncrementSentCount();

        assert(!optRequest->GetRoute().empty());
        assert(optRequest->GetExtension<Message::Extension::Awaitable>());
        auto const& [identifier, registration] = *endpoint;
        auto const& scheduler = registration.GetMessageAction();
        bool const success = scheduler(*m_spIdentifier, Network::MessageVariant{ optRequest->GetPack() });
        if (success) { return optTrackerKey; }
    }

    return {};
}

//----------------------------------------------------------------------------------------------------------------------

bool Peer::Proxy::ScheduleSend(Message::Application::Builder& builder) const
{
    auto const endpoint = GetOrSetPreferredEndpoint(builder);
    if (endpoint == m_endpoints.end()) { return false; }

    builder.SetDestination(*m_spIdentifier); // Set the destination as the the one represented by this proxy. 

    if (auto const optMessage = builder.ValidatedBuild(); optMessage) [[likely]] {
        m_statistics.IncrementSentCount();

        assert(!optMessage->GetRoute().empty());
        auto const& [identifier, registration] = *endpoint;
        auto const& scheduler = registration.GetMessageAction();
        return scheduler(*m_spIdentifier, Network::MessageVariant{ optMessage->GetPack() });
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
        return scheduler(*m_spIdentifier, Network::MessageVariant{ std::move(message) });
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
        return scheduler(*m_spIdentifier, Network::MessageVariant{ spSharedPack });
    }

    return false;
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
        std::scoped_lock lock{ m_endpointsMutex, m_associatedMutex };
        auto [itr, isEndpointEmplaced] = m_endpoints.try_emplace(
            // Registered Endpoint Key
            identifier,
            // Registered Endpoint Arguments
            weak_from_this(), identifier, protocol, address, scheduler, disconnector);

        BindSecurityContext(itr->second.GetWritableMessageContext());

       auto [jtr, isAddressEmplaced] = m_associated.emplace(address, true);
       if (!isAddressEmplaced) { jtr->second = true; }
    }

    if (auto const spResolutionService = m_wpResolutionService.lock(); spResolutionService) {
        spResolutionService->OnEndpointRegistered(shared_from_this(), identifier, address);
    }
}

//----------------------------------------------------------------------------------------------------------------------

void Peer::Proxy::WithdrawEndpoint(Network::Endpoint::Identifier identifier, WithdrawalCause cause)
{
    bool reset = false;
    RegisteredEndpoints::node_type extracted;
    {
        std::scoped_lock lock{ m_endpointsMutex, m_associatedMutex };
        if (extracted = m_endpoints.extract(identifier); !extracted) { return; }
        assert(m_associated.contains(extracted.mapped().GetAddress()));
        m_associated[extracted.mapped().GetAddress()] = false;
        reset = m_endpoints.empty();
    }

    if (auto const spResolutionService = m_wpResolutionService.lock(); spResolutionService) {
        auto const& address = extracted.mapped().GetAddress();
        spResolutionService->OnEndpointWithdrawn(shared_from_this(), identifier, address, cause);
    }

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

void Peer::Proxy::AssociateRemote(Network::RemoteAddress const& remote)
{
    std::scoped_lock lock{ m_associatedMutex };
    [[maybe_unused]] auto const [jtr, emplaced] = m_associated.emplace(remote, false);
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

bool Peer::Proxy::IsEndpointRegistered(std::string_view uri) const
{
    std::scoped_lock lock{ m_endpointsMutex };
    return std::ranges::any_of(m_endpoints, [&uri] (auto const& entry) {
        return uri == entry.second.GetAddress().GetUri();
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

bool Peer::Proxy::IsRemoteAssociated(Network::RemoteAddress const& remote) const
{
    std::shared_lock lock{ m_associatedMutex };
    return m_associated.find(remote) != m_associated.end();
}

//----------------------------------------------------------------------------------------------------------------------

bool Peer::Proxy::IsRemoteAssociated(Network::Protocol protocol, std::string_view address) const
{
    std::shared_lock lock{ m_associatedMutex };
    return FindRemote(protocol, address) != m_associated.end();
}

//----------------------------------------------------------------------------------------------------------------------

bool Peer::Proxy::IsRemoteAssociated(std::string_view uri) const
{
    std::shared_lock lock{ m_associatedMutex };
    return FindRemote(uri) != m_associated.end();
}

//----------------------------------------------------------------------------------------------------------------------

bool Peer::Proxy::IsRemoteConnected(Network::RemoteAddress const& remote) const
{
    std::shared_lock lock{ m_associatedMutex };
    if (auto const itr = m_associated.find(remote); itr != m_associated.end()) {
        return itr->second;
    }
    return false;
}

//----------------------------------------------------------------------------------------------------------------------

bool Peer::Proxy::IsRemoteConnected(Network::Protocol protocol, std::string_view address) const
{
    std::shared_lock lock{ m_associatedMutex };
    if (auto const itr = FindRemote(protocol, address); itr != m_associated.end()) {
        return itr->second;
    }
    return false;
}

//----------------------------------------------------------------------------------------------------------------------

bool Peer::Proxy::IsRemoteConnected(std::string_view uri) const
{
    std::shared_lock lock{ m_associatedMutex };
    if (auto const itr = FindRemote(uri); itr != m_associated.end()) {
        return itr->second;
    }
    return false;
}

//----------------------------------------------------------------------------------------------------------------------

bool Peer::Proxy::ForEach(EndpointReader const& reader) const
{
    std::scoped_lock lock{ m_endpointsMutex };
    for (auto const& [identifier, registration] : m_endpoints) {
        if (reader(registration) != CallbackIteration::Continue) { return false; }
    }
    return true;
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
        [this] (std::unique_ptr<Security::CipherPackage>&& upCipherPackage) {
            std::scoped_lock lock{ m_securityMutex };
            m_upCipherPackage = std::move(upCipherPackage);
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
                    auto const spResolutionService = m_wpResolutionService.lock();
                    auto const& address = registration.GetAddress();
                    assert(spResolutionService);
                    spResolutionService->OnEndpointRegistered(shared_from_this(), identifier, address);
                }
            }
        });

    return true; 
}

//----------------------------------------------------------------------------------------------------------------------

void Peer::Proxy::DetachResolver()
{
    m_upResolver.reset(); // Cleanup the attached the resolver, this should be called after the peer has been resolved.
}

//----------------------------------------------------------------------------------------------------------------------

bool Peer::Proxy::StartExchange(
    Security::ExchangeRole role, std::shared_ptr<Node::ServiceProvider> spServiceProvider)
{
    {
        std::shared_lock lock{ m_securityMutex };
        if (m_upResolver) [[unlikely]] { return false; }
    }

    auto upResolver = std::make_unique<Resolver>();
    switch (role) {
        case Security::ExchangeRole::Acceptor: {
            bool const result = upResolver->SetupExchangeAcceptor(spServiceProvider);
            return result && AttachResolver(std::move(upResolver));
        }
        case Security::ExchangeRole::Initiator: {
            assert(false); // Currently, we only support starting an accepting resolver. 
            auto optRequest = upResolver->SetupExchangeInitiator(spServiceProvider);
            if (!optRequest || !AttachResolver(std::move(upResolver))) { return false; }
            return ScheduleSend(std::numeric_limits<std::uint32_t>::max(), std::move(*optRequest));
        }
        default: assert(false); return false; // What is this?
    }

    return false; // How did we fallthrough to here? 
}

//----------------------------------------------------------------------------------------------------------------------

Security::State Peer::Proxy::GetAuthorization() const { return m_authorization; }

//----------------------------------------------------------------------------------------------------------------------

bool Peer::Proxy::IsFlagged() const { return m_authorization == Security::State::Flagged; }

//----------------------------------------------------------------------------------------------------------------------

bool Peer::Proxy::IsAuthorized() const { return m_authorization == Security::State::Authorized; }

//----------------------------------------------------------------------------------------------------------------------

Peer::Proxy::RegisteredEndpoints::const_iterator Peer::Proxy::GetOrSetPreferredEndpoint(
    Message::Application::Builder& builder) const
{
    // If the builder does not have a context  attached, use one of the known registered endpoints. Otherwise, ensure
    // the one that has been provided if it is valid. 
    RegisteredEndpoints::const_iterator endpoint = m_endpoints.end();
    if (auto const& context = builder.GetContext(); context == Message::Context{}) {
        endpoint = FetchPreferredEndpoint();
        auto const& [identifier, registration] = *endpoint;
        builder.SetContext(registration.GetMessageContext());
    } else {
        endpoint = m_endpoints.find(context.GetEndpointIdentifier());
    }
    return endpoint;
}

//----------------------------------------------------------------------------------------------------------------------

Peer::Proxy::RegisteredEndpoints::const_iterator Peer::Proxy::FetchPreferredEndpoint() const
{
    return m_endpoints.begin(); // Note: Using preferred endpoint will be future work. 
}

//----------------------------------------------------------------------------------------------------------------------

Peer::Proxy::AssociatedAddresses::const_iterator Peer::Proxy::FindRemote(
    Network::Protocol protocol, std::string_view address) const
{
    return std::find_if(m_associated.begin(), m_associated.end(), [&](auto const& entry) {
        return protocol == entry.first.GetProtocol() && address == entry.first.GetAuthority();
    });
}

//----------------------------------------------------------------------------------------------------------------------

Peer::Proxy::AssociatedAddresses::const_iterator Peer::Proxy::FindRemote(std::string_view uri) const
{
    return std::find_if(m_associated.begin(), m_associated.end(), [&](auto const& entry) {
        return uri == entry.first.GetUri();
    });
}

//----------------------------------------------------------------------------------------------------------------------

void Peer::Proxy::BindSecurityContext(Message::Context& context)
{
    context.BindEncryptionHandlers(
        [this] (auto plaintext, auto& destination) -> Security::Encryptor::result_type {
            std::shared_lock lock{ m_securityMutex };
            if (!m_upCipherPackage) [[unlikely]] { return {}; }
            return m_upCipherPackage->Encrypt(plaintext, destination);
        },
        [this] (auto ciphertext) -> Security::Decryptor::result_type {
            std::shared_lock lock{ m_securityMutex };
            if (!m_upCipherPackage) [[unlikely]] { return {}; }
            return m_upCipherPackage->Decrypt(ciphertext);
        },
        [this] (std::size_t size) -> Security::EncryptedSizeGetter::result_type {
            std::shared_lock lock{ m_securityMutex };
            if (!m_upCipherPackage) [[unlikely]] { return size; }
            return m_upCipherPackage->GetSuite().GetEncryptedSize(size);
        });

    context.BindSignatureHandlers(
        [this] (auto& buffer) -> Security::Signator::result_type {
            std::shared_lock lock{ m_securityMutex };
            if (!m_upCipherPackage) [[unlikely]] { return false; }
            return m_upCipherPackage->Sign(buffer);
        },
        [this] (auto buffer) -> Security::Verifier::result_type {
            std::shared_lock lock{ m_securityMutex };
            if (!m_upCipherPackage) [[unlikely]] { return Security::VerificationStatus::Failed; }
            return m_upCipherPackage->Verify(buffer);
        },
        [this] () -> Security::SignatureSizeGetter::result_type {
            std::shared_lock lock{ m_securityMutex };
            if (!m_upCipherPackage) [[unlikely]] { return 0; }
            return m_upCipherPackage->GetSuite().GetSignatureSize();
        });
}

//----------------------------------------------------------------------------------------------------------------------

template<>
void Peer::Proxy::SetResolutionService<InvokeContext::Test>(
    std::weak_ptr<IResolutionService> const& wpResolutionService)
{
    m_wpResolutionService = wpResolutionService;
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
void Peer::Proxy::SetAuthorization<InvokeContext::Test>(Security::State state) { m_authorization = state; }

//----------------------------------------------------------------------------------------------------------------------

template <>
void Peer::Proxy::AttachCipherPackage<InvokeContext::Test>(std::unique_ptr<Security::CipherPackage>&& upCipherPackage)
{
    assert(!m_upResolver);
    m_upCipherPackage = std::move(upCipherPackage);
    assert(m_upCipherPackage);

    // Ensure any registered endpoints have their message contexts updated to the new mediator's security context.
    std::scoped_lock endpointsLock{ m_endpointsMutex };
    for (auto& [identifier, registration]: m_endpoints) {
        BindSecurityContext(registration.GetWritableMessageContext());
    }
}

//----------------------------------------------------------------------------------------------------------------------

template <>
void Peer::Proxy::RegisterSilentEndpoint<InvokeContext::Test>(
    Network::Endpoint::Identifier identifier,
    Network::Protocol protocol,
    Network::RemoteAddress const& address,
    Network::MessageAction const& scheduler)
{
    std::scoped_lock lock{ m_endpointsMutex, m_associatedMutex };
    auto [itr, result] =  m_endpoints.try_emplace(
        identifier, weak_from_this(), identifier, protocol, address, scheduler);

    // By default, bind simple passthrough for the context handlers. 
    auto& context = itr->second.GetWritableMessageContext();

    context.BindEncryptionHandlers(
        [] (auto plaintext, auto& destination) -> Security::Encryptor::result_type {
            std::ranges::copy(plaintext, std::back_inserter(destination));
            return true;
        },
        [] (auto ciphertext) -> Security::Decryptor::result_type {
            return Security::Buffer(ciphertext.begin(), ciphertext.end());
        },
        [] (std::size_t size) -> Security::EncryptedSizeGetter::result_type {
            return size;
        });

    context.BindSignatureHandlers(
        [] (auto&) -> Security::Signator::result_type  { return true; },
        [] (auto const&) -> Security::Verifier::result_type { return Security::VerificationStatus::Success; },
        [] () -> Security::SignatureSizeGetter::result_type { return 0; });

    auto [jtr, isAddressEmplaced] = m_associated.emplace(address, true);
    if (!isAddressEmplaced) { jtr->second = true; }
}

//----------------------------------------------------------------------------------------------------------------------

template <>
void Peer::Proxy::WithdrawSilentEndpoint<InvokeContext::Test>(
    Network::Endpoint::Identifier identifier, [[maybe_unused]] Network::Protocol protocol)
{
    std::scoped_lock lock{ m_endpointsMutex, m_associatedMutex };
    if (auto const itr = m_endpoints.find(identifier); itr != m_endpoints.end()) {
        m_endpoints.erase(itr);
        m_associated[itr->second.GetAddress()] = false; // The remote is now disconnected. 
    }
}

//----------------------------------------------------------------------------------------------------------------------
