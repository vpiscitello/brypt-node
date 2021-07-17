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
    RuntimeContext context,
    Configuration::Options::Endpoints const& endpoints,
    Event::SharedPublisher const& spEventPublisher,
    IPeerMediator* const pPeerMediator,
    IBootstrapCache const* const pBootstrapCache)
    : m_active(false)
    , m_spEventPublisher(spEventPublisher)
    , m_endpointsMutex()
    , m_endpoints()
    , m_cacheMutex()
    , m_protocols()
    , m_bindings()
{
    assert(m_spEventPublisher);
    spEventPublisher->Advertise(Event::Type::CriticalNetworkFailure);

    // Register listeners to watch for error states that might trigger a critical network shutdown. 
    spEventPublisher->Subscribe<Event::Type::BindingFailed>(
        [this, context] (Network::Endpoint::Identifier, Network::BindingAddress const&)
        { OnBindingFailed(context); });

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

bool Network::Manager::IsRegisteredAddress(Address const& address) const
{
    auto const MatchBinding = [&address] (auto const& binding) -> bool { return address.equivalent(binding); };
    constexpr auto ProjectBinding = [] (auto const& pair) -> auto { return pair.second; };

    std::shared_lock lock(m_cacheMutex);
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
    constexpr auto StartEndpoint = [] (auto const& spEndpoint)
    {
        assert(spEndpoint);
        spEndpoint->Startup();
    };

    std::scoped_lock lock(m_endpointsMutex);
    std::ranges::for_each(m_endpoints | std::views::values, StartEndpoint);
    m_active = true; // Reset the signal to ensure shutdowns can be handled this cycle. 
}

//----------------------------------------------------------------------------------------------------------------------

void Network::Manager::Shutdown()
{
    constexpr auto ShutdownEndpoint = [] (auto const& spEndpoint) 
    {
        assert(spEndpoint);
        [[maybe_unused]] bool const stopped = spEndpoint->Shutdown();
        assert(stopped);
    };

    std::scoped_lock lock(m_endpointsMutex);
    std::ranges::for_each(m_endpoints | std::views::values, ShutdownEndpoint);
    m_active = false; // Indicate all resources are currently in a stopped state. 
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
    auto const FindEndpoint = [&protocol, &operation] (auto const& spEndpoint) -> bool
    {
        assert(spEndpoint);
        return spEndpoint->GetProtocol() == protocol && spEndpoint->GetOperation() == operation;
    };

    std::shared_lock lock(m_endpointsMutex);
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

Network::BindingAddress Network::Manager::GetEndpointBinding(Endpoint::Identifier identifier) const
{
    auto const MatchFn = [&identifier] (auto key) -> bool { return identifier == key; };
    constexpr auto ProjectionFn = [] (auto const& pair) -> auto { return pair.first; };
   
    std::shared_lock lock(m_cacheMutex);
    if (auto const itr = std::ranges::find_if(m_bindings, MatchFn, ProjectionFn); itr != m_bindings.end()) {
        return itr->second;
    }
    return {};
}

//----------------------------------------------------------------------------------------------------------------------

std::size_t Network::Manager::ActiveEndpointCount() const
{
    constexpr auto const IsActive = [] (auto const& spEndpoint) -> bool 
    {
        assert(spEndpoint);
        return spEndpoint->IsActive();
    };

    std::shared_lock lock(m_endpointsMutex);
    return std::ranges::count_if(m_endpoints | std::views::values, IsActive);
}

//----------------------------------------------------------------------------------------------------------------------

std::size_t Network::Manager::ActiveProtocolCount() const
{
    ProtocolSet protocols;
    auto const AppendActiveProtocol = [&protocols] (auto const& spEndpoint) 
    {
        assert(spEndpoint);
        if (spEndpoint->IsActive()) { protocols.emplace(spEndpoint->GetProtocol()); }
    };

    std::shared_lock lock(m_endpointsMutex);
    std::ranges::for_each(m_endpoints | std::views::values, AppendActiveProtocol);
    return protocols.size();
}

//----------------------------------------------------------------------------------------------------------------------

bool Network::Manager::ScheduleBind(BindingAddress const& binding)
{
    auto const FindEndpoint = [protocol = binding.GetProtocol()] (auto const& spEndpoint) -> bool
    {
        assert(spEndpoint);
        return spEndpoint->GetProtocol() == protocol && spEndpoint->GetOperation() == Operation::Server;
    };

    std::shared_lock lock(m_endpointsMutex);
    auto const view = m_endpoints | std::views::values;
    if (auto const itr = std::ranges::find_if(view, FindEndpoint); itr != view.end()) {
        auto const& spEndpoint = *itr;
        return spEndpoint->ScheduleBind(binding);
    }

    return false;
}

//----------------------------------------------------------------------------------------------------------------------

bool Network::Manager::ScheduleConnect(RemoteAddress const& address)
{
    return ScheduleConnect(Network::RemoteAddress{ address }, nullptr);
}

//----------------------------------------------------------------------------------------------------------------------

bool Network::Manager::ScheduleConnect(RemoteAddress&& address)
{
    return ScheduleConnect(std::move(address), nullptr);
}

//----------------------------------------------------------------------------------------------------------------------

bool Network::Manager::ScheduleConnect(RemoteAddress&& address, Node::SharedIdentifier const& spIdentifier)
{
    auto const FindEndpoint = [protocol = address.GetProtocol()] (auto const& spEndpoint) -> bool
    {
        assert(spEndpoint);
        return spEndpoint->GetProtocol() == protocol && spEndpoint->GetOperation() == Operation::Client;
    };

    std::shared_lock lock(m_endpointsMutex);
    auto const view = m_endpoints | std::views::values;
    if (auto const itr = std::ranges::find_if(view, FindEndpoint); itr != view.end()) {
        auto const& spEndpoint = *itr;
        return spEndpoint->ScheduleConnect(std::move(address), spIdentifier);
    }

    return false;
}

//----------------------------------------------------------------------------------------------------------------------

void Network::Manager::Initialize(
    Configuration::Options::Endpoints const& endpoints,
    Event::SharedPublisher const& spEventPublisher,
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
    Configuration::Options::Endpoint const& options,
    Event::SharedPublisher const& spEventPublisher,
    IPeerMediator* const pPeerMediator,
    IBootstrapCache const* const pBootstrapCache)
{
    assert(options.GetProtocol() == Protocol::TCP);

    // Add the server based endpoint
    {
        auto spServer = Endpoint::Factory(Protocol::TCP, Operation::Server, spEventPublisher, this, pPeerMediator);
        [[maybe_unused]] bool const scheduled = spServer->ScheduleBind(options.GetBinding());
        assert(scheduled);
        // Cache the binding such that clients can check the anticipated bindings before servers report an update.
        // This method is used over UpdateBinding because the shared lock is preferable to a recursive lock given 
        // reads are more common. This is the work around to reacquiring the lock made during initialization. 
        UpdateBindingCache(spServer->GetIdentifier(), options.GetBinding());
        m_endpoints.emplace(spServer->GetIdentifier(), std::move(spServer));
    }

    // Add the client based endpoint
    {
        auto spClient = Endpoint::Factory(Protocol::TCP, Operation::Client, spEventPublisher, this, pPeerMediator);
        if (pBootstrapCache) { local::ConnectBootstraps(spClient, pBootstrapCache); }
        m_endpoints.emplace(spClient->GetIdentifier(), std::move(spClient));
    }

    m_protocols.emplace(options.GetProtocol());
}

//----------------------------------------------------------------------------------------------------------------------

void Network::Manager::UpdateBindingCache(Endpoint::Identifier identifier, BindingAddress const& binding)
{
    // Note: The cache mutex must be locked before calling this method. We don't acquire the lock here to allow a 
    // single lock during initialization. 
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

void Network::Manager::OnBindingFailed(RuntimeContext context)
{
    switch (context) {
        // When operating as a background process, the end user is able to determine how to resolve the error. 
        case RuntimeContext::Background: break;
        // It's not currently possible to determine the error's resolution when operating in the foreground 
        // We must shutdown and indicate that critical error occured that shutdown the network. 
        case RuntimeContext::Foreground: OnCriticalError(); break;
        default: assert(false); break; // What is this? 
    }
}

//----------------------------------------------------------------------------------------------------------------------

void Network::Manager::OnEndpointShutdown(RuntimeContext context, ShutdownCause cause)
{
    switch (cause) {
        // We can ignore requested shutdowns as they are procedural. 
        case ShutdownCause::ShutdownRequest: return;
        // Let the binding error handler determine what should happen. 
        case ShutdownCause::BindingFailed: OnBindingFailed(context); return;
        // Any unexpected errors that cause an endpoint shutdown are handled as critical network errors. 
        case ShutdownCause::UnexpectedError: break;
        default: assert(false); return; // What is this?
    }

    OnCriticalError(); // The shutdown was determiend to not be procedural or recoverable. 
}

//----------------------------------------------------------------------------------------------------------------------

void Network::Manager::OnCriticalError()
{
    if (!m_active) { return; } // We only need handle the first instance of a critical error. 

    Shutdown(); // Shutdown all processing. The assumption being the user should investigate the cause. 
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
    pCache->ForEachBootstrap(upEndpoint->GetProtocol(), 
        [&upEndpoint] (RemoteAddress const& bootstrap) -> CallbackIteration
        { upEndpoint->ScheduleConnect(bootstrap); return CallbackIteration::Continue; });
}

//----------------------------------------------------------------------------------------------------------------------
