//----------------------------------------------------------------------------------------------------------------------
// File: Manager.cpp
// Description: 
//----------------------------------------------------------------------------------------------------------------------
#include "Manager.hpp"
//----------------------------------------------------------------------------------------------------------------------
#include "Address.hpp"
#include "Endpoint.hpp"
#include "BryptNode/ServiceProvider.hpp"
#include "Components/Configuration/BootstrapService.hpp"
#include "Components/Event/Publisher.hpp"
#include "Components/Network/LoRa/Endpoint.hpp"
#include "Components/Network/TCP/Endpoint.hpp"
#include "Components/Scheduler/TaskService.hpp"
//----------------------------------------------------------------------------------------------------------------------
#include <algorithm>
#include <ranges>
//----------------------------------------------------------------------------------------------------------------------

Network::Manager::Manager(RuntimeContext context, std::shared_ptr<Node::ServiceProvider> const& spServiceProvider)
    : m_active(false)
    , m_context(context)
    , m_spEventPublisher(spServiceProvider->Fetch<Event::Publisher>())
    , m_spTaskService(spServiceProvider->Fetch<Scheduler::TaskService>())
    , m_endpointsMutex()
    , m_endpoints()
    , m_cacheMutex()
    , m_protocols()
    , m_bindings()
{
    assert(m_spEventPublisher);
    assert(m_spTaskService);
    m_spEventPublisher->Advertise(Event::Type::CriticalNetworkFailure);

    // Register listeners to watch for error states that might trigger a critical network shutdown. 
    using BindingFailure = Event::Message<Event::Type::BindingFailed>::Cause;
    m_spEventPublisher->Subscribe<Event::Type::BindingFailed>(
        [this, context] (Network::Endpoint::Identifier, Network::BindingAddress const&, BindingFailure) {
            OnBindingFailed(context);
        });

    m_spEventPublisher->Subscribe<Event::Type::EndpointStopped>(
        [this, context] (Endpoint::Identifier, BindingAddress const&, ShutdownCause cause) {
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
    auto const matches = [&address] (auto const& binding) -> bool { return address.Equivalent(binding); };

    std::shared_lock lock(m_cacheMutex);
    return std::ranges::any_of(m_bindings | std::views::elements<1>, matches);
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
    std::shared_ptr<Node::ServiceProvider> const& spServiceProvider)
{
    return std::ranges::all_of(endpoints, [this, &spServiceProvider] (auto const& endpoint) {
        return Attach(endpoint, spServiceProvider);
    });
}

//----------------------------------------------------------------------------------------------------------------------

bool Network::Manager::Attach(
    Configuration::Options::Endpoint const& endpoint,
    std::shared_ptr<Node::ServiceProvider> const& spServiceProvider)
{
    std::scoped_lock lock(m_endpointsMutex, m_cacheMutex);
    auto const protocol = endpoint.GetProtocol();

    // Currently, we don't support attaching endpoints if there is an existing set for the given protocol. 
    if (auto const itr = m_protocols.find(endpoint.GetProtocol()); itr != m_protocols.end()) { return false; }

    // Create the endpoint resources required for the given protocol. 
    switch (protocol) {
        case Protocol::TCP: { CreateTcpEndpoints(endpoint, spServiceProvider); } break;
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
    auto const detach = [&protocol] (auto const& entry) -> bool { 
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

Network::Manager::SharedEndpoint Network::Manager::GetEndpoint(Protocol protocol) const
{
    auto const matches = [&protocol] (auto const& spEndpoint) -> bool {
        assert(spEndpoint);
        return spEndpoint->GetProtocol() == protocol; // TODO: If multi endpoint per protocol is supported this needs to be smarter. 
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

std::size_t Network::Manager::ForEach(BindingCallback const& callback) const
{
    std::shared_lock lock(m_cacheMutex);

    std::size_t read = 0;
    for (auto const& [identifier, binding] : m_bindings) {
        ++read; // Increment the number of binding addresses read. 
        if (callback(identifier, binding) != CallbackIteration::Continue) { break; }
    }

    return read;
}

//----------------------------------------------------------------------------------------------------------------------

bool Network::Manager::ScheduleBind(BindingAddress const& binding)
{
    auto const matches = [protocol = binding.GetProtocol()] (auto const& spEndpoint) -> bool {
        assert(spEndpoint);
        return spEndpoint->GetProtocol() == protocol; // TODO: If multi endpoint per protocol is supported this needs to be smarter. 
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
        return spEndpoint->GetProtocol() == protocol; // TODO: If multi endpoint per protocol is supported this needs to be smarter. 
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
    std::shared_ptr<Node::ServiceProvider> const& spServiceProvider)
{
    assert(options.GetProtocol() == Protocol::TCP);
    std::shared_ptr<IResolutionService> const spResolutionService{ spServiceProvider->Fetch<IResolutionService>() };

    Endpoint::Properties const properties{ options };
    auto spEndpoint = std::make_shared<TCP::Endpoint>(properties);
    spEndpoint->Register(this);
    spEndpoint->Register(m_spEventPublisher);
    spEndpoint->Register(spResolutionService.get());

    [[maybe_unused]] bool const scheduled = spEndpoint->ScheduleBind(options.GetBinding());
    assert(scheduled);

    // If the endpoint should connect to the stored bootstraps, schedule a one-shot task to be run in the core. 
    if (options.UseBootstraps()) {
        auto const wpBootstrapCache = spServiceProvider->Fetch<BootstrapService>();
        auto const connect = [wpClient = std::weak_ptr<IEndpoint>{ spEndpoint }, wpBootstrapCache]() {
            auto const spClient = wpClient.lock();
            auto const spBootstrapCache = wpBootstrapCache.lock();
            if (spClient && spBootstrapCache) {
                spBootstrapCache->ForEachBootstrap(Protocol::TCP, [&spClient](auto const& bootstrap) -> auto {
                    [[maybe_unused]] bool const scheduled = spClient->ScheduleConnect(bootstrap);
                    assert(scheduled);
                    return CallbackIteration::Continue;
                });
            }
        };
        m_spTaskService->Schedule(connect);
    }

    // Cache the binding such that clients can check the anticipated bindings before servers report an update.
    // This method is used over UpdateBinding because the shared lock is preferable to a recursive lock given 
    // reads are more common. This is the work around to reacquiring the lock made during initialization. 
    UpdateBindingCache(spEndpoint->GetIdentifier(), options.GetBinding());

    m_protocols.emplace(options.GetProtocol());
    m_endpoints.emplace(spEndpoint->GetIdentifier(), std::move(spEndpoint));
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

template<>
void Network::Manager::RegisterEndpoint<InvokeContext::Test>(
    Configuration::Options::Endpoint const& options, SharedEndpoint const& spEndpoint)
{
    std::scoped_lock lock{ m_endpointsMutex };
    UpdateBindingCache(spEndpoint->GetIdentifier(), options.GetBinding());
    m_protocols.emplace(options.GetProtocol());
    m_endpoints.emplace(spEndpoint->GetIdentifier(), spEndpoint);
}

//----------------------------------------------------------------------------------------------------------------------
