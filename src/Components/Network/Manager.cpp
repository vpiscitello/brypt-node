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
#include "Components/Scheduler/TaskService.hpp"
//----------------------------------------------------------------------------------------------------------------------
#include <algorithm>
#include <ranges>
//----------------------------------------------------------------------------------------------------------------------

Network::Manager::Manager(
    RuntimeContext context,
    std::shared_ptr<Scheduler::TaskService> const& spTaskService,    
    Event::SharedPublisher const& spEventPublisher)
    : m_active(false)
    , m_context(context)
    , m_spEventPublisher(spEventPublisher)
    , m_spTaskService(spTaskService)
    , m_endpointsMutex()
    , m_endpoints()
    , m_cacheMutex()
    , m_protocols()
    , m_bindings()
{
    assert(m_spEventPublisher);
    spEventPublisher->Advertise(Event::Type::CriticalNetworkFailure);

    // Register listeners to watch for error states that might trigger a critical network shutdown. 
    using BindingFailure = Event::Message<Event::Type::BindingFailed>::Cause;
    spEventPublisher->Subscribe<Event::Type::BindingFailed>(
        [this, context] (Network::Endpoint::Identifier, Network::BindingAddress const&, BindingFailure) {
            OnBindingFailed(context);
        });

    spEventPublisher->Subscribe<Event::Type::EndpointStopped>(
        [this, context] (Endpoint::Identifier, Protocol, Operation, ShutdownCause cause) {
            OnEndpointShutdown(context, cause);
        });
}

//----------------------------------------------------------------------------------------------------------------------

Network::Manager::~Manager()
{
    Shutdown();
}

//----------------------------------------------------------------------------------------------------------------------

bool Network::Manager::IsRegisteredAddress(Address const& address) const
{
    auto const matches = [&address] (auto const& binding) -> bool { return address.equivalent(binding); };
    constexpr auto projection = [] (auto const& pair) -> auto { return pair.second; };

    std::shared_lock lock(m_cacheMutex);
    return std::ranges::any_of(m_bindings, matches, projection);
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
    constexpr auto startup = [] (auto const& spEndpoint) { 
        assert(spEndpoint);
        spEndpoint->Startup();
    };
    std::ranges::for_each(m_endpoints | std::views::values, startup);
    m_active = true; // Reset the signal to ensure shutdowns can be handled this cycle. 
}

//----------------------------------------------------------------------------------------------------------------------

void Network::Manager::Shutdown()
{
    std::scoped_lock lock(m_endpointsMutex);
    
    constexpr auto shutdown = [] (auto const& spEndpoint) {
        assert(spEndpoint);
        [[maybe_unused]] bool const stopped = spEndpoint->Shutdown();
        assert(stopped);
    };

    std::ranges::for_each(m_endpoints | std::views::values, shutdown);
    m_active = false; // Indicate all resources are currently in a stopped state. 
}

//----------------------------------------------------------------------------------------------------------------------

bool Network::Manager::Attach(
    Configuration::Options::Endpoints const& endpoints,
    IPeerMediator* const pPeerMediator,
    IBootstrapCache const* const pBootstrapCache)
{
    return std::ranges::all_of(endpoints, [this, pPeerMediator, pBootstrapCache] (auto const& endpoint) {
        return Attach(endpoint, pPeerMediator, pBootstrapCache);
    });
}

//----------------------------------------------------------------------------------------------------------------------

bool Network::Manager::Attach(
    Configuration::Options::Endpoint const& endpoint,
    IPeerMediator* const pPeerMediator,
    IBootstrapCache const* const pBootstrapCache)
{
    std::scoped_lock lock(m_endpointsMutex, m_cacheMutex);
    auto const protocol = endpoint.GetProtocol();

    // Currently, we don't support attaching endpoints if there is an existing set for the given protocol. 
    if (auto const itr = m_protocols.find(endpoint.GetProtocol()); itr != m_protocols.end()) { return false; }

    // Create the endpoint resources required for the given protocol. 
    switch (protocol) {
        case Protocol::TCP: {
            CreateTcpEndpoints(endpoint, m_spEventPublisher, pPeerMediator, pBootstrapCache);
        } break;
        default: break; // No other protocols have implemented endpoints
    }

    // If the manager has already been started, spin-up the new endpoints for the given protocol. 
    if (m_active) {
        constexpr auto startup = [] (auto const& spEndpoint) { spEndpoint->Startup(); };
        auto const matches = [&protocol] (auto const spEndpoint) { return spEndpoint->GetProtocol() == protocol; };
        std::ranges::for_each(m_endpoints | std::views::values | std::views::filter(matches), startup);
    }

    return true;
}

//----------------------------------------------------------------------------------------------------------------------

bool Network::Manager::Detach(Configuration::Options::Endpoint const& options)
{
    std::scoped_lock lock(m_endpointsMutex, m_cacheMutex);
    auto const protocol = options.GetProtocol();

    // If there are no endpoint's attached for the given protocol, there is nothing to do. 
    if (auto const itr = m_protocols.find(options.GetProtocol()); itr == m_protocols.end()) { return false; }

    // Setup the detach method for erase_if, before removing it from our container we need to shut it down explicitly
    // in case another resource is keeping the shared_ptr alive. 
    auto const detach = [&protocol, &bindings = m_bindings] (auto const& entry) -> bool { 
        auto const& [identifier, spEndpoint] = entry;
        if (spEndpoint->GetProtocol() != protocol) { return false; }
        [[maybe_unused]] bool const stopped = spEndpoint->Shutdown();
        return true;
    };
    
    [[maybe_unused]] std::size_t const detached = std::erase_if(m_endpoints, detach);
    assert(detached != 0); // We should always detach some endpoint if we reach this point. 
    m_protocols.erase(protocol);

     // Unset the active flag if all endpoints have been detached.
    if (m_endpoints.empty() && m_active) { m_active = false; }

    return true;
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
    auto const matches = [&protocol, &operation] (auto const& spEndpoint) -> bool {
        assert(spEndpoint);
        return spEndpoint->GetProtocol() == protocol && spEndpoint->GetOperation() == operation;
    };

    std::shared_lock lock(m_endpointsMutex);
    auto const view = m_endpoints | std::views::values;
    if (auto const itr = std::ranges::find_if(view, matches); itr != view.end()) { return *itr; }
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
    auto const matches = [&identifier] (auto key) -> bool { return identifier == key; };
    constexpr auto project = [] (auto const& pair) -> auto { return pair.first; };
   
    std::shared_lock lock(m_cacheMutex);
    if (auto const itr = std::ranges::find_if(m_bindings, matches, project); itr != m_bindings.end()) {
        return itr->second;
    }
    return {};
}

//----------------------------------------------------------------------------------------------------------------------

std::size_t Network::Manager::ActiveEndpointCount() const
{
    constexpr auto const active = [] (auto const& spEndpoint) -> bool {
        assert(spEndpoint);
        return spEndpoint->IsActive();
    };

    std::shared_lock lock(m_endpointsMutex);
    return std::ranges::count_if(m_endpoints | std::views::values, active);
}

//----------------------------------------------------------------------------------------------------------------------

std::size_t Network::Manager::ActiveProtocolCount() const
{
    ProtocolSet protocols;
    auto const append = [&protocols] (auto const& spEndpoint) {
        assert(spEndpoint);
        if (spEndpoint->IsActive()) { protocols.emplace(spEndpoint->GetProtocol()); }
    };

    std::shared_lock lock(m_endpointsMutex);
    std::ranges::for_each(m_endpoints | std::views::values, append);
    return protocols.size();
}

//----------------------------------------------------------------------------------------------------------------------

bool Network::Manager::ScheduleBind(BindingAddress const& binding)
{
    auto const matches = [protocol = binding.GetProtocol()] (auto const& spEndpoint) -> bool {
        assert(spEndpoint);
        return spEndpoint->GetProtocol() == protocol && spEndpoint->GetOperation() == Operation::Server;
    };

    std::shared_lock lock(m_endpointsMutex);
    auto const view = m_endpoints | std::views::values;
    if (auto const itr = std::ranges::find_if(view, matches); itr != view.end()) {
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
    auto const matches = [protocol = address.GetProtocol()] (auto const& spEndpoint) -> bool {
        assert(spEndpoint);
        return spEndpoint->GetProtocol() == protocol && spEndpoint->GetOperation() == Operation::Client;
    };

    std::shared_lock lock(m_endpointsMutex);
    auto const view = m_endpoints | std::views::values;
    if (auto const itr = std::ranges::find_if(view, matches); itr != view.end()) {
        auto const& spEndpoint = *itr;
        return spEndpoint->ScheduleConnect(std::move(address), spIdentifier);
    }

    return false;
}

//----------------------------------------------------------------------------------------------------------------------

void Network::Manager::CreateTcpEndpoints(
    Configuration::Options::Endpoint const& options,
    Event::SharedPublisher const& spEventPublisher,
    IPeerMediator* const pPeerMediator,
    IBootstrapCache const* const pBootstrapCache)
{
    assert(options.GetProtocol() == Protocol::TCP);

    // Add the server based endpoint
    {
        std::shared_ptr<IEndpoint> spServer = Endpoint::Factory(
            Protocol::TCP, Operation::Server, spEventPublisher, this, pPeerMediator);
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
        std::shared_ptr<IEndpoint> spClient = Endpoint::Factory(
            Protocol::TCP, Operation::Client, spEventPublisher, this, pPeerMediator);
        m_endpoints.emplace(spClient->GetIdentifier(), spClient);

        // If we have been provided a bootstrap cache, schedule a one-shot task to be run in the core. 
        if (pBootstrapCache) {
            auto const connect = [spClient, pBootstrapCache] () {
                assert(spClient && pBootstrapCache);
                pBootstrapCache->ForEachBootstrap(Protocol::TCP, [&spClient] (auto const& bootstrap) -> auto { 
                    spClient->ScheduleConnect(bootstrap);
                    return CallbackIteration::Continue;
                });
            };
            m_spTaskService->Schedule(connect);
        }
    }

    m_protocols.emplace(options.GetProtocol());
}

//----------------------------------------------------------------------------------------------------------------------

void Network::Manager::UpdateBindingCache(Endpoint::Identifier identifier, BindingAddress const& binding)
{
    // Note: The cache mutex must be locked before calling this method. We don't acquire the lock here to allow a 
    // single lock during initialization. 
    auto const matches = [&identifier] (auto key) -> bool { return identifier == key; };
    constexpr auto project = [] (auto const& pair) -> auto { return pair.first; };
    
    // Note: The BindingCache is optimized for faster lookup than updating the binding. The idea is that lookups 
    // are a lot more common than registering or updating a binding. 
    // If the identifier is found updated the associated binding. Otherwise, emplace a new cache item. 
    if (auto itr = std::ranges::find_if(m_bindings, matches, project); itr != m_bindings.end()) {
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
