//----------------------------------------------------------------------------------------------------------------------
// File: ProxyStore.cpp
// Description:
//----------------------------------------------------------------------------------------------------------------------
#include "ProxyStore.hpp"
#include "BryptIdentifier/BryptIdentifier.hpp"
#include "BryptMessage/ApplicationMessage.hpp"
#include "BryptMessage/PlatformMessage.hpp"
#include "BryptNode/ServiceProvider.hpp"
#include "Components/Awaitable/TrackingService.hpp"
#include "Components/Event/Events.hpp"
#include "Components/Event/Publisher.hpp"
#include "Components/Security/SecurityDefinitions.hpp"
#include "Components/Scheduler/Delegate.hpp"
#include "Components/Scheduler/Registrar.hpp"
#include "Components/State/NodeState.hpp"
#include "Interfaces/ConnectProtocol.hpp"
#include "Interfaces/MessageSink.hpp"
#include "Interfaces/PeerObserver.hpp"
#include "Interfaces/SecurityStrategy.hpp"
//----------------------------------------------------------------------------------------------------------------------
#include <cassert>
#include <random>
//----------------------------------------------------------------------------------------------------------------------

//----------------------------------------------------------------------------------------------------------------------
namespace {
namespace local {
//----------------------------------------------------------------------------------------------------------------------

//----------------------------------------------------------------------------------------------------------------------
} // local namespace
} // namespace
//----------------------------------------------------------------------------------------------------------------------

//----------------------------------------------------------------------------------------------------------------------
// Description: 
//----------------------------------------------------------------------------------------------------------------------
Peer::ProxyStore::ProxyStore(
    Security::Strategy strategy,
    std::shared_ptr<Scheduler::Registrar> const& spRegistrar,
    std::shared_ptr<Node::ServiceProvider> const& spServiceProvider)
    : m_spDelegate()
    , m_spNodeIdentifier()
    , m_spEventPublisher(spServiceProvider->Fetch<Event::Publisher>())
    , m_strategyType(strategy)
    , m_observersMutex()
    , m_observers()
    , m_resolvingMutex()
    , m_resolving()
    , m_resolved()
    , m_peersMutex()
    , m_peers()
    , m_active(0)
    , m_spTrackingService(spServiceProvider->Fetch<Awaitable::TrackingService>())
    , m_spConnectProtocol(spServiceProvider->Fetch<IConnectProtocol>())
    , m_wpServiceProvider(spServiceProvider)
{
    assert(m_strategyType != Security::Strategy::Invalid);

    m_spDelegate = spRegistrar->Register<Peer::ProxyStore>([this] (Scheduler::Frame const&) -> std::size_t {
        return Execute();  // Dispatch any fulfilled awaiting messages since this last cycle. 
    }); 
    assert(m_spDelegate);

    if (auto const spState = spServiceProvider->Fetch<NodeState>().lock(); spState) {
        m_spNodeIdentifier = spState->GetNodeIdentifier();
    }
    assert(m_spNodeIdentifier);

    {
        using enum Event::Type;
        m_spEventPublisher->Advertise({ PeerConnected, PeerDisconnected });
    }
}

//----------------------------------------------------------------------------------------------------------------------

void Peer::ProxyStore::RegisterObserver(IPeerObserver* const observer)
{
    std::scoped_lock lock(m_observersMutex);
    m_observers.emplace(observer);
}

//----------------------------------------------------------------------------------------------------------------------

void Peer::ProxyStore::UnpublishObserver(IPeerObserver* const observer)
{
    std::scoped_lock lock(m_observersMutex);
    m_observers.erase(observer);
}

//----------------------------------------------------------------------------------------------------------------------

Peer::ProxyStore::OptionalRequest Peer::ProxyStore::DeclareResolvingPeer(
    Network::RemoteAddress const& address, Node::SharedIdentifier const& spPeerIdentifier)
{
    auto const spServiceProvider = m_wpServiceProvider.lock();
    if (!spServiceProvider) { return {}; }

    // Disallow endpoints from connecting to the same uri. If an endpoint has connection retry logic, it should store
    // the connection request message. However, there exists a race  condition when the peer wakes up while the endpoint
    // is still not sure a peer exists at that particular uri. In this case the peer may send a bootstrap request causing 
    // the endpoint to check if we are currently resolving that uri. 
    std::scoped_lock lock(m_resolvingMutex);
    if (auto const itr = m_resolving.find(address); itr != m_resolving.end()) { return {}; }

    // If the we are provided an identifier for the peer, prefer short circuiting the exchange and send a hearbeat 
    // request to instantiate the endpoint's connection. Otherwise, create a resolver to initiate the exchange. 
    if (spPeerIdentifier) {
        if (auto const optShortCircuitRequest = GenerateShortCircuitRequest(spPeerIdentifier); optShortCircuitRequest) {
            return optShortCircuitRequest;
        }
    }
    
    // Store the resolver such that when the endpoint links the peer it can be attached to the real peer proxy. 
    auto upResolver = std::make_unique<Resolver>(Security::Context::Unique);
    auto optRequest = upResolver->SetupExchangeInitiator(m_strategyType, spServiceProvider);
    assert(optRequest);

    m_resolving[address] = std::move(upResolver);

    return optRequest; 
}

//----------------------------------------------------------------------------------------------------------------------

void Peer::ProxyStore::RescindResolvingPeer(Network::RemoteAddress const& address)
{
    std::scoped_lock lock(m_resolvingMutex);
    [[maybe_unused]] std::size_t const result = m_resolving.erase(address);
    assert(result == 1); // This function should only be called if the peer has been declared. 
}

//----------------------------------------------------------------------------------------------------------------------

std::shared_ptr<Peer::Proxy> Peer::ProxyStore::LinkPeer(
    Node::Identifier const& identifier, Network::RemoteAddress const& address)
{
    // If the provided peer has an identifier that matches an already tracked peer, the tracked peer needs to be 
    // returned to the caller. Otherwise, a new peer needs to be constructed, tracked, and returned to the caller. 
    std::scoped_lock lock(m_resolvingMutex, m_peersMutex);
    if (auto const itr = m_peers.find(identifier); itr != m_peers.end()) {
        std::shared_ptr<Peer::Proxy> spUnified;
        m_peers.modify(itr, [&spUnified] (std::shared_ptr<Peer::Proxy>& spTracked) {
            assert(spTracked); spUnified = spTracked;
        });

        // If the peer exists for the given identifier, but there are no registered endpoints the peer is reconnecting
        // and an exchange is needed to establish keys. 
        if (spUnified->RegisteredEndpointCount() == 0) { AttachOrCreateExchange(spUnified, address); }

        return spUnified; // Return the unified peer to the endpoint. 
    }

    return CreatePeer(identifier, address); // Create the new peer proxy if one could not be found. 
}

//----------------------------------------------------------------------------------------------------------------------

void Peer::ProxyStore::OnEndpointRegistered(
    std::shared_ptr<Peer::Proxy> const& spProxy,
    Network::Endpoint::Identifier identifier,
    Network::RemoteAddress const& address)
{
    // If this peer has already been marked as authorized, then dispatch the new address. Otherwise, the notification
    // is deferred until an exchange is successfully completed. 
    if (spProxy->GetAuthorization() == Security::State::Authorized) {
        using enum Event::Type;
        NotifyObservers(&IPeerObserver::OnRemoteConnected, identifier, address);
        m_spEventPublisher->Publish<PeerConnected>(spProxy, address);
       
        // If this is the first endpoint being registered, mark it such that the attached resolver can be cleaned up 
        // and increment the count of active peers. 
        if (spProxy->RegisteredEndpointCount() == 1) { 
            std::scoped_lock lock{ m_resolvingMutex };
            m_resolved.emplace_back(spProxy); // Store the resolved peer such that the attached resolver can be cleaned up. 
            m_spDelegate->OnTaskAvailable(); // Notify the scheduler that we have a tasks that can be executed. 
            ++m_active;
        }
    }
}

//----------------------------------------------------------------------------------------------------------------------

void Peer::ProxyStore::OnEndpointWithdrawn(
    std::shared_ptr<Peer::Proxy> const& spProxy,
    Network::Endpoint::Identifier identifier,
    Network::RemoteAddress const& address, 
    WithdrawalCause cause)
{
    // Withdrawing a registed endpoint is only a dispatchable event when not caused by a shutdown request and the
    // peer been authorized (indicating a prior connect event has been dispatched for the peer). 
    auto const authorization = spProxy->GetAuthorization();
    bool dispatchable = cause != WithdrawalCause::NetworkShutdown && authorization == Security::State::Authorized;
    if (dispatchable) {
        using enum Event::Type;
        NotifyObservers(&IPeerObserver::OnRemoteDisconnected, identifier, address);
        m_spEventPublisher->Publish<PeerDisconnected>(spProxy, address, cause);
        if (spProxy->RegisteredEndpointCount() == 0) { --m_active; }
    }
}

//----------------------------------------------------------------------------------------------------------------------

bool Peer::ProxyStore::ForEach(IdentifierReadFunction const& callback, Filter filter) const
{
    std::shared_lock lock(m_peersMutex);
    for (auto const& spProxy: m_peers) {
        bool isIncluded = true;
        switch (filter) {
            case Filter::Active: { isIncluded = spProxy->IsActive(); } break;
            case Filter::Inactive: { isIncluded = !spProxy->IsActive(); } break;
            case Filter::None: { isIncluded = true; } break;
            default: assert(false); // What is this?
        }

        if (isIncluded && callback(spProxy->GetIdentifier()) != CallbackIteration::Continue) { break; }
    }

    return true;
}

//----------------------------------------------------------------------------------------------------------------------

std::size_t Peer::ProxyStore::ActiveCount() const
{
    std::scoped_lock lock{ m_peersMutex };
    return m_active;
}

//----------------------------------------------------------------------------------------------------------------------

std::size_t Peer::ProxyStore::InactiveCount() const
{
    std::scoped_lock lock{ m_peersMutex };
    return m_peers.size() - m_active;
}

//----------------------------------------------------------------------------------------------------------------------

std::size_t Peer::ProxyStore::ObservedCount() const
{
    std::scoped_lock lock{ m_peersMutex };
    return m_peers.size();
}

//----------------------------------------------------------------------------------------------------------------------

std::size_t Peer::ProxyStore::ResolvingCount() const
{
    std::shared_lock lock(m_resolvingMutex);
    return m_resolving.size();
}

//----------------------------------------------------------------------------------------------------------------------

std::shared_ptr<Peer::Proxy> Peer::ProxyStore::Find(Node::Identifier const& identifier) const
{
    std::shared_lock lock{ m_peersMutex };
    if (auto const itr = m_peers.find(identifier); itr != m_peers.end()) {
        return *itr;
    }
    return nullptr;
}

//----------------------------------------------------------------------------------------------------------------------

std::shared_ptr<Peer::Proxy> Peer::ProxyStore::Find(std::string_view identifier) const
{
    std::shared_lock lock{ m_peersMutex };
    auto const& index = m_peers.template get<ExternalIndex>();
    if (auto const itr = index.find(identifier.data()); itr != index.end()) {
        return *itr;
    }
    return nullptr;
}

//----------------------------------------------------------------------------------------------------------------------

bool Peer::ProxyStore::Contains(Node::Identifier const& identifier) const
{
    std::shared_lock lock{ m_peersMutex };
    if (auto const itr = m_peers.find(identifier); itr != m_peers.end()) {
        return true;
    }
    return false;
}

//----------------------------------------------------------------------------------------------------------------------

bool Peer::ProxyStore::Contains(std::string_view identifier) const
{
    std::shared_lock lock{ m_peersMutex };
    auto const& index = m_peers.template get<ExternalIndex>();
    if (auto const itr = index.find(identifier.data()); itr != index.end()) {
        return true;
    }
    return false;
}

//----------------------------------------------------------------------------------------------------------------------

bool Peer::ProxyStore::IsActive(Node::Identifier const& identifier) const
{
    std::shared_lock lock{ m_peersMutex };
    if (auto const itr = m_peers.find(identifier); itr != m_peers.end()) {
        return itr->get()->IsActive();
    }
    return false;
}

//----------------------------------------------------------------------------------------------------------------------

bool Peer::ProxyStore::IsActive(std::string_view identifier) const
{
    std::shared_lock lock{ m_peersMutex };
    auto const& index = m_peers.template get<ExternalIndex>();
    if (auto const itr = index.find(identifier.data()); itr != index.end()) {
        return itr->get()->IsActive();
    }
    return false;
}

//----------------------------------------------------------------------------------------------------------------------

bool Peer::ProxyStore::ForEach(ForEachFunction const& callback, Filter filter) const
{
    std::shared_lock lock(m_peersMutex);
    for (auto const& spProxy: m_peers) {
        bool included = false;
        switch (filter) {
            case Filter::Active: { included = spProxy->IsActive(); } break;
            case Filter::Inactive: { included = !spProxy->IsActive(); } break;
            case Filter::None: { included = true; } break;
            default: assert(false); // What is this?
        }

        if (included && callback(spProxy) != CallbackIteration::Continue) { return false; }
    }

    return true;
}

//----------------------------------------------------------------------------------------------------------------------

bool Peer::ProxyStore::Dispatch(
    std::string_view identifier, std::string_view route, Message::Payload&& payload) const
{
    std::shared_lock lock{ m_peersMutex };
    auto const& index = m_peers.template get<ExternalIndex>();
    if (auto const itr = index.find(identifier.data()); itr != index.end()) {
        auto const spProxy = *itr;

        auto builder = Message::Application::Parcel::GetBuilder()
            .SetSource(*m_spNodeIdentifier)
            .SetRoute(route)
            .SetPayload(std::move(payload));

        return spProxy->ScheduleSend(builder);
    }
    return false;
}

//----------------------------------------------------------------------------------------------------------------------

Peer::ProxyStore::ClusterDispatchResult Peer::ProxyStore::Notify(
    Message::Destination destination,
    std::string_view route, 
    Message::Payload const& payload,
    Predicate const& predicate) const
{
    std::shared_lock lock{ m_peersMutex };
    if (m_peers.empty()) { return {}; }

    std::size_t dispatched = 0;
    for (auto const& spProxy : m_peers) {
        bool const included = (predicate) ? predicate(*spProxy) : spProxy->IsActive();
        if (included) {
            auto builder = Message::Application::Parcel::GetBuilder()
                .SetSource(*m_spNodeIdentifier)
                .SetRoute(route)
                .SetPayload(Message::Payload{ payload });

            switch(destination) {
                case Message::Destination::Cluster: builder.MakeClusterMessage(); break;
                case Message::Destination::Network: builder.MakeNetworkMessage(); break;
                default: assert(false); break;
            }

            bool const success = spProxy->ScheduleSend(builder);
            if (success) { ++dispatched; }
        }
    }

    return dispatched;
}

//----------------------------------------------------------------------------------------------------------------------

std::optional<Awaitable::TrackerKey> Peer::ProxyStore::Request(
    std::string_view identifier,
    std::string_view route, 
    Message::Payload&& payload,
    Action::OnResponse const& onResponse,
    Action::OnError const& onError) const
{
    std::shared_lock lock{ m_peersMutex };
    
    auto const& index = m_peers.template get<ExternalIndex>();
    if (auto const itr = index.find(identifier.data()); itr != index.end()) {
        auto const spProxy = *itr;

        auto builder = Message::Application::Parcel::GetBuilder()
            .SetSource(*m_spNodeIdentifier)
            .SetRoute(route)
            .SetPayload(std::move(payload));

        return spProxy->Request(builder, onResponse, onError);
    }

    return {};
}

//----------------------------------------------------------------------------------------------------------------------

Peer::ProxyStore::ClusterRequestResult Peer::ProxyStore::Request(
    Message::Destination destination,
    std::string_view route, 
    Message::Payload const& payload,
    Action::OnResponse const& onResponse,
    Action::OnError const& onError,
    Predicate const& predicate) const
{
    std::shared_lock lock{ m_peersMutex };
    if (m_peers.empty()) { return {}; }
    
    auto const optResult = m_spTrackingService->StageRequest(*m_spNodeIdentifier, m_active, onResponse, onError);
    if (!optResult) { return {}; }

    auto const& [key, correlator] = *optResult;

    std::size_t requested = 0;
    for (auto const& spProxy : m_peers) {
        bool const included = (predicate) ? predicate(*spProxy) : spProxy->IsActive();
        if (included) {
            using namespace Message;

            [[maybe_unused]] bool const correlated = correlator(spProxy->GetIdentifier());
            assert(correlated);

            auto builder = Message::Application::Parcel::GetBuilder()
                .SetSource(*m_spNodeIdentifier)
                .SetRoute(route)
                .SetPayload(Message::Payload{ payload });

            switch(destination) {
                case Message::Destination::Cluster: builder.MakeClusterMessage(); break;
                case Message::Destination::Network: builder.MakeNetworkMessage(); break;
                default: assert(false); break;
            }
            
            builder.BindExtension<Extension::Awaitable>(Extension::Awaitable::Request, key);

            bool const success = spProxy->ScheduleSend(builder);
            if (success) { ++requested; }
        }
    }

    if (requested == 0) {
        m_spTrackingService->Cancel(key);
        return std::make_pair(Awaitable::TrackerKey{}, 0);
    }

    return std::make_pair(key, requested);
}

//----------------------------------------------------------------------------------------------------------------------

bool Peer::ProxyStore::ScheduleDisconnect(Node::Identifier const& identifier) const
{
    std::shared_lock lock(m_peersMutex);
    if (auto const itr = m_peers.find(identifier); itr != m_peers.end()) { 
        auto const spProxy = *itr;
        return spProxy->ScheduleDisconnect();
    }
    return false;
}

//----------------------------------------------------------------------------------------------------------------------

bool Peer::ProxyStore::ScheduleDisconnect(std::string_view identifier) const
{
    std::shared_lock lock(m_peersMutex);
    auto const& index = m_peers.template get<ExternalIndex>();
    if (auto const itr = index.find(identifier.data()); itr != index.end()) {
        auto const spProxy = *itr;
        return spProxy->ScheduleDisconnect();
    }
    return false;
}

//----------------------------------------------------------------------------------------------------------------------

std::size_t Peer::ProxyStore::ScheduleDisconnect(Network::Address const& address) const
{
    std::shared_lock lock(m_peersMutex);
    return std::ranges::count_if(m_peers, [&address] (auto const& spProxy) {
        if (spProxy->IsEndpointRegistered(address)) { return spProxy->ScheduleDisconnect(); }
        return false;
    });
}

//----------------------------------------------------------------------------------------------------------------------

std::size_t Peer::ProxyStore::Execute()
{
    auto const resolved = ( std::scoped_lock{ m_resolvingMutex }, std::exchange(m_resolved, {}) );
    for (auto const& wpResolved : resolved) {
        if (auto const spResolved = wpResolved.lock()) {
            spResolved->DetachResolver();
        }
    }
    return resolved.size();
}

//----------------------------------------------------------------------------------------------------------------------

Peer::ProxyStore::OptionalRequest Peer::ProxyStore::GenerateShortCircuitRequest(
    Node::SharedIdentifier const& spPeerIdentifier) const
{
    // Note: The peers mutex must be locked before calling this method. 
    assert(spPeerIdentifier && spPeerIdentifier->IsValid());

    // If the peer is not currently tracked, a exchange short circuit message cannot be generated. 
    if(auto const itr = m_peers.find(*spPeerIdentifier); itr == m_peers.end()) { return {}; }

    // Currently, the short circuiting method is to notify the peer via a heatbeat request. 
    auto const optRequest = Message::Platform::Parcel::GetBuilder()
        .SetSource(*m_spNodeIdentifier)
        .SetDestination(*spPeerIdentifier)
        .MakeHeartbeatRequest()
        .ValidatedBuild();
    assert(optRequest);

    return optRequest->GetPack();
}

//----------------------------------------------------------------------------------------------------------------------

std::shared_ptr<Peer::Proxy> Peer::ProxyStore::CreatePeer(
    Node::Identifier const& identifier, Network::RemoteAddress const& address)
{
    if (auto const spServiceProvider = m_wpServiceProvider.lock(); spServiceProvider) {
        // Note: The resolving and peers mutexes must be locked before calling this method. 
        auto const spProxy = Proxy::CreateInstance(identifier, spServiceProvider);
        AttachOrCreateExchange(spProxy, address);
        m_peers.emplace(spProxy);

        return spProxy; // Provide the newly created proxy to the caller. 
    }

    return {};
}

//----------------------------------------------------------------------------------------------------------------------

void Peer::ProxyStore::AttachOrCreateExchange(std::shared_ptr<Proxy> const& spProxy, Network::RemoteAddress const& address)
{
    // If the endpoint has declared the address as a resolving peer, this implies that we were the connection initiator. 
    // In this case, we need to attach the external resolver to the full proxy to handle incoming responses. 
    // Otherwise, we are accepting a new request from an unknown address, in which we need to tell the proxy to start 
    // an resolver to process the messages. 
    if (auto itr = m_resolving.find(address); itr != m_resolving.end()) {
        [[maybe_unused]] bool const result = spProxy->AttachResolver(std::move(itr->second));
        assert(result);
        m_resolving.erase(itr);
        return;
    }

    if (auto const spServiceProvider = m_wpServiceProvider.lock(); spServiceProvider) {
        [[maybe_unused]] bool const result = spProxy->StartExchange(
            m_strategyType, Security::Role::Acceptor, spServiceProvider);
        assert(result);
    }
}

//----------------------------------------------------------------------------------------------------------------------

template<typename FunctionType, typename...Args>
void Peer::ProxyStore::NotifyObservers(FunctionType const& function, Args&&...args)
{
    for (auto itr = m_observers.cbegin(); itr != m_observers.cend();) {
        auto const& observer = *itr; // Get a refernce to the current observer pointer.
        // If the observer is no longer valid erase the dangling pointer from the set.
        if (!observer) { itr = m_observers.erase(itr); continue; } 
        auto const binder = std::bind(function, observer, std::forward<Args>(args)...);
        binder(); // Call the observer with the provided arguments. 
        ++itr; // Move to the next observer in the set. 
    }
}

//----------------------------------------------------------------------------------------------------------------------

template<typename FunctionType, typename...Args>
void Peer::ProxyStore::NotifyObserversConst(FunctionType const& function, Args&&...args) const
{
    for (auto const& observer: m_observers) {
        auto const binder = std::bind(function, observer, std::forward<Args>(args)...);
        binder();
    }
}

//----------------------------------------------------------------------------------------------------------------------
