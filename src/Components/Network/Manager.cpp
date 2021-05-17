//----------------------------------------------------------------------------------------------------------------------
// File: Manager.cpp
// Description: 
//----------------------------------------------------------------------------------------------------------------------
#include "Manager.hpp"
//----------------------------------------------------------------------------------------------------------------------
#include "Address.hpp"
#include "Endpoint.hpp"
#include "Components/Event/Publisher.hpp"
#include "Components/Network/LoRa/Endpoint.hpp"
#include "Components/Network/TCP/Endpoint.hpp"
//----------------------------------------------------------------------------------------------------------------------
#include <algorithm>
#include <ranges>
//----------------------------------------------------------------------------------------------------------------------

//----------------------------------------------------------------------------------------------------------------------
namespace {
namespace local {
//----------------------------------------------------------------------------------------------------------------------

void ConnectBootstraps(std::unique_ptr<Network::IEndpoint> const& spEndpoint, IBootstrapCache const* const pCache);

//----------------------------------------------------------------------------------------------------------------------
} // local namespace
} // namespace
//----------------------------------------------------------------------------------------------------------------------

Network::Manager::Manager(
    Configuration::EndpointsSet const& endpoints,
    std::shared_ptr<Event::Publisher> const& spEventPublisher,
    IPeerMediator* const pPeerMediator,
    IBootstrapCache const* const pBootstrapCache,
    RuntimeContext context)
    : m_spEventPublisher(spEventPublisher)
    , m_endpointsMutex()
    , m_endpoints()
    , m_cacheMutex()
    , m_protocols()
    , m_bindings()
{
    assert(m_spEventPublisher);
    spEventPublisher->Advertise(Event::Type::CriticalNetworkFailure);

    // Register an listener for when endpoints report a stop, this handler will watch for any critical errpr states 
    spEventPublisher->Subscribe<Event::Type::EndpointStopped>(
        [this, context] (Endpoint::Identifier, Protocol, Operation, ShutdownCause cause)
        { OnEndpointShutdown(context, cause); });

    Initialize(endpoints, spEventPublisher, pPeerMediator, pBootstrapCache);
}

//----------------------------------------------------------------------------------------------------------------------

Network::Manager::~Manager()
{
    Shutdown();
}

//----------------------------------------------------------------------------------------------------------------------

bool Network::Manager::IsAddressRegistered(Address const& address) const
{
    std::shared_lock lock(m_cacheMutex);
    auto const MatchBinding = [&address] (auto const& binding) -> bool { return address.equivalent(binding); };
    constexpr auto ProjectBinding = [] (auto const& pair) -> auto { return pair.second; };
    return std::ranges::any_of(m_bindings, MatchBinding, ProjectBinding);
}

//----------------------------------------------------------------------------------------------------------------------

void Network::Manager::UpdateBinding(Endpoint::Identifier identifier, BindingAddress const& binding)
{
    std::scoped_lock lock(m_cacheMutex);
    UpdateBindingCache(identifier, binding);

}

//----------------------------------------------------------------------------------------------------------------------

void Network::Manager::Startup()
{
    std::scoped_lock lock(m_endpointsMutex);
    constexpr auto StartEndpoint = [] (auto const& spEndpoint)
    {
        assert(spEndpoint);
        spEndpoint->Startup();
    };
    std::ranges::for_each(m_endpoints | std::views::values, StartEndpoint);
}

//----------------------------------------------------------------------------------------------------------------------

void Network::Manager::Shutdown()
{
    std::scoped_lock lock(m_endpointsMutex);
    constexpr auto ShutdownEndpoint = [] (auto const& spEndpoint) 
    {
        assert(spEndpoint);
        [[maybe_unused]] bool const stopped = spEndpoint->Shutdown();
        assert(stopped);
    };
    std::ranges::for_each(m_endpoints | std::views::values, ShutdownEndpoint);
}

//----------------------------------------------------------------------------------------------------------------------

Network::Manager::SharedEndpoint Network::Manager::GetEndpoint(Endpoint::Identifier identifier) const
{
    std::shared_lock lock(m_endpointsMutex);
    auto const itr = m_endpoints.find(identifier);
    return (itr != m_endpoints.end()) ? itr->second : nullptr;
}

//----------------------------------------------------------------------------------------------------------------------

Network::Manager::SharedEndpoint Network::Manager::GetEndpoint(Protocol protocol, Operation operation) const
{
    std::shared_lock lock(m_endpointsMutex);
    auto const FindEndpoint = [&protocol, &operation] (auto const& spEndpoint) -> bool
    {
        assert(spEndpoint);
        return spEndpoint->GetProtocol() == protocol && operation == spEndpoint->GetOperation();
    };
    auto const view = m_endpoints | std::views::values;
    if (auto const itr = std::ranges::find_if(view, FindEndpoint); itr != view.end()) { return *itr; }
    return nullptr;
}

//----------------------------------------------------------------------------------------------------------------------

Network::ProtocolSet Network::Manager::GetEndpointProtocols() const
{
    std::shared_lock lock(m_cacheMutex);
    return m_protocols;
}

//----------------------------------------------------------------------------------------------------------------------

std::size_t Network::Manager::ActiveEndpointCount() const
{
    std::shared_lock lock(m_endpointsMutex);
    constexpr auto const IsActive = [] (auto const& spEndpoint) -> bool 
    {
        assert(spEndpoint);
        return spEndpoint->IsActive();
    };
    return std::ranges::count_if(m_endpoints | std::views::values, IsActive);
}

//----------------------------------------------------------------------------------------------------------------------

std::size_t Network::Manager::ActiveProtocolCount() const
{
    std::shared_lock lock(m_endpointsMutex);
    ProtocolSet protocols;
    auto const AppendActiveProtocol = [&protocols] (auto const& spEndpoint) 
    {
        assert(spEndpoint);
        if (spEndpoint->IsActive()) { protocols.emplace(spEndpoint->GetProtocol()); }
    };
    std::ranges::for_each(m_endpoints | std::views::values, AppendActiveProtocol);
    return protocols.size();
}

//----------------------------------------------------------------------------------------------------------------------

void Network::Manager::Initialize(
    Configuration::EndpointsSet const& endpoints,
    std::shared_ptr<Event::Publisher> const& spEventPublisher,
    IPeerMediator* const pPeerMediator,
    IBootstrapCache const* const pBootstrapCache)
{
    std::scoped_lock lock(m_endpointsMutex, m_cacheMutex);
    // Iterate through the provided configurations to setup the endpoints for the given technolgy
    for (auto const& options: endpoints) {
        auto const protocol = options.GetProtocol();
        // If the protocol has not already been configured then continue with the setup. 
        // This function should only be called once per application run, there shouldn't be a reason
        // to re-initilize a protocol as the endpoints should exist until appliction termination.
        if (auto const itr = m_protocols.find(protocol); itr == m_protocols.end()) {         
            switch (protocol) {
                case Protocol::TCP: {
                    InitializeTCPEndpoints(options, spEventPublisher, pPeerMediator, pBootstrapCache);
                } break;
                default: break; // No other protocols have implemented endpoints
            }
        }
    }
}

//----------------------------------------------------------------------------------------------------------------------

void Network::Manager::InitializeTCPEndpoints(
    Configuration::EndpointOptions const& options,
    std::shared_ptr<Event::Publisher> const& spEventPublisher,
    IPeerMediator* const pPeerMediator,
    IBootstrapCache const* const pBootstrapCache)
{
    assert(options.GetProtocol() == Protocol::TCP);

    // Add the server based endpoint
    {
        auto spServer = Endpoint::Factory(Protocol::TCP, Operation::Server, spEventPublisher, this, pPeerMediator);
        bool const scheduled = spServer->ScheduleBind(options.GetBinding());
        assert(scheduled);
        // Cache the binding such that clients can check the anticipated bindings before servers report an update.
        // This method is used over UpdateBinding because the shared lock is preferable to a recursive lock given 
        // reads are more common. This is the work around to reacquiring the lock made during initialization. 
        UpdateBindingCache(spServer->GetEndpointIdentifier(), options.GetBinding());
        m_endpoints.emplace(spServer->GetEndpointIdentifier(), std::move(spServer));
    }

    // Add the client based endpoint
    {
        auto spClient = Endpoint::Factory(Protocol::TCP, Operation::Client, spEventPublisher, this, pPeerMediator);
        if (pBootstrapCache) { local::ConnectBootstraps(spClient, pBootstrapCache); }
        m_endpoints.emplace(spClient->GetEndpointIdentifier(), std::move(spClient));
    }

    m_protocols.emplace(options.GetProtocol());
}

//----------------------------------------------------------------------------------------------------------------------

void Network::Manager::UpdateBindingCache(Endpoint::Identifier identifier, BindingAddress const& binding)
{
    auto const MatchIdentifier = [&identifier] (auto key) -> bool { return identifier == key; };
    constexpr auto ProjectIdentifier = [] (auto const& pair) -> auto { return pair.first; };
    
    // Note: The BindingCache is optimized for faster lookup than updating the binding. The idea is that lookups 
    // are a lot more common than registering or updating a binding. 
    // If the identifier is found updated the associated binding. Otherwise, emplace a new cache item. 
    if (auto itr = std::ranges::find_if(m_bindings, MatchIdentifier, ProjectIdentifier); itr != m_bindings.end()) {
        itr->second = binding;
    } else {
        m_bindings.emplace_back(identifier, binding);
    }
}

//----------------------------------------------------------------------------------------------------------------------

void Network::Manager::OnEndpointShutdown(RuntimeContext context, ShutdownCause cause)
{
    switch (cause) {
        // Only binding failures that cause an endpoint shutdown ae handled as critical network errors if
        // the process if in the foreground. Otherwise, the user can determine how to handle the failure. 
        // (e.g. using a different binding, shutting down, etc.)
        case ShutdownCause::BindingFailed: {
            if (context == RuntimeContext::Foreground) { break; }
        } return;
        // Any unexpected errors that cause an endpoint shutdown are handled as critical network errors. 
        case ShutdownCause::UnexpectedError: break;
        case ShutdownCause::ShutdownRequest: return; // We can ignore requested shutdowns as they are procedural. 
        default: assert(false); return; // What is this?
    }

    Shutdown(); // Shutdown all processing. The assumption being the user should investigate the cause. 
    
    // The reported shutdown has caused a critical network failure and can not be recovered from.
    m_spEventPublisher->Publish<Event::Type::CriticalNetworkFailure>();
}

//----------------------------------------------------------------------------------------------------------------------

void local::ConnectBootstraps(
    std::unique_ptr<Network::IEndpoint> const& upEndpoint, IBootstrapCache const* const pCache)
{
    using namespace Network;
    assert(upEndpoint && upEndpoint->GetOperation() == Operation::Client);
    assert(pCache);

    // Iterate through the provided bootstraps for the endpoint and schedule a connect
    // for each peer in the list.
    pCache->ForEachCachedBootstrap(upEndpoint->GetProtocol(), 
        [&upEndpoint] (RemoteAddress const& bootstrap) -> CallbackIteration
        {
            upEndpoint->ScheduleConnect(bootstrap);
            return CallbackIteration::Continue;
        });
}

//----------------------------------------------------------------------------------------------------------------------
