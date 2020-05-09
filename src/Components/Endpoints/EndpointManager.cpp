//------------------------------------------------------------------------------------------------
// File: Endpoint.cpp
// Description: 
//------------------------------------------------------------------------------------------------
#include "EndpointManager.hpp"
#include "Endpoint.hpp"
//------------------------------------------------------------------------------------------------

CEndpointManager::CEndpointManager()
    : m_endpoints()
    , m_technologies()
    , m_observersMutex()
    , m_observers()
{
}

//------------------------------------------------------------------------------------------------

CEndpointManager::~CEndpointManager()
{
    Shutdown();
}

//------------------------------------------------------------------------------------------------

void CEndpointManager::Initialize(
    NodeUtils::NodeIdType id,
    IMessageSink* const messageSink,
    Configuration::EndpointConfigurations const& configurations,
    Configuration::OptionalEndpointPeersMap const& optEndpointBootstraps)
{
    // Iterate through the provided configurations to setup the endpoints for the given technolgy
    for (auto const& options: configurations) {
        auto const technology = options.GetTechnology();
        // Get all of the endpoints that may already present for the current technology
        // If the technology has not already been configured then continue with the setup. 
        // This function should only be called once per application run, there shouldn't be a reason
        // to re-initilize a technology as the endpoints should exist until appliction termination.
        if (auto const [begin, end] = m_endpoints.equal_range(technology); begin == m_endpoints.end()) {
            // Attempt to find bootstrap peers in the provided endpoint bootstraps. If the technology
            // has bootstraps available forward it to the function during technology initialization.
            Configuration::OptionalPeersMap optBootstraps = {};
            if (optEndpointBootstraps) {
                if (auto const itr = optEndpointBootstraps->find(technology); itr != optEndpointBootstraps->end()) {
                    optBootstraps = itr->second;
                }
            }            
            switch (technology) {
                case Endpoints::TechnologyType::Direct: {
                    InitializeDirectEndpoints(id, options, messageSink, optBootstraps);
                } break;
                case Endpoints::TechnologyType::TCP: {
                    InitializeTCPEndpoints(id, options, messageSink, optBootstraps);
                } break;
                case Endpoints::TechnologyType::StreamBridge: {
                    InitializeStreamBridgeEndpoints(id, options, messageSink);
                } break;
                default: break; // No other technologies have implemented endpoints
            }
        }
    }
}

//------------------------------------------------------------------------------------------------

void CEndpointManager::ConnectBootstraps(
    std::shared_ptr<CEndpoint> const& spEndpoint,
    Configuration::PeersMap const& bootstraps)
{
    // If the given endpoint id not able to connect to peers, don't do anything.
    if (spEndpoint->GetOperation() != Endpoints::OperationType::Client) {
        return;
    }

    // Iterate through the provided bootstraps for the endpoint and schedule a connect
    // for each peer in the list.
    for (auto const& [id, bootstrap]: bootstraps) {
        spEndpoint->ScheduleConnect(bootstrap.GetEntry());
    }
}

//------------------------------------------------------------------------------------------------

void CEndpointManager::Startup()
{
    for (auto const& [technology, endpoint]: m_endpoints) {
        endpoint->Startup();
    }
}

//------------------------------------------------------------------------------------------------

void CEndpointManager::Shutdown()
{
    for (auto const& [technology, endpoint]: m_endpoints) {
        if (endpoint) {
            [[maybe_unused]] bool const bStopped = endpoint->Shutdown();
        }
    }
}

//------------------------------------------------------------------------------------------------

Endpoints::TechnologySet CEndpointManager::GetEndpointTechnologies() const
{
    return m_technologies;
}

//------------------------------------------------------------------------------------------------

std::uint32_t CEndpointManager::ActiveEndpointCount() const
{
    std::uint32_t count = 0;
    for (auto const& [technology, endpoint] : m_endpoints) {
        if (endpoint && endpoint->IsActive()) {
            ++count;
        }
    }
    return count;
}

//------------------------------------------------------------------------------------------------

std::uint32_t CEndpointManager::ActiveTechnologyCount() const
{
    Endpoints::TechnologySet technologies;
    for (auto const& [technology, endpoint] : m_endpoints) {
        if (endpoint && endpoint->IsActive()) {
            technologies.emplace(technology);
        }
    }
    return technologies.size();
}

//------------------------------------------------------------------------------------------------

void CEndpointManager::RegisterObserver(IPeerObserver* const observer)
{
    std::scoped_lock lock(m_observersMutex);
    m_observers.emplace(observer);
}

//------------------------------------------------------------------------------------------------

void CEndpointManager::UnpublishObserver(IPeerObserver* const observer)
{
    std::scoped_lock lock(m_observersMutex);
    m_observers.erase(observer);
}

//------------------------------------------------------------------------------------------------

void CEndpointManager::ForwardPeerConnectionStateChange(
    CPeer const& peer,
    ConnectionState change)
{
    NotifyObservers(&IPeerObserver::HandlePeerConnectionStateChange, peer, change);
}

//------------------------------------------------------------------------------------------------

void CEndpointManager::InitializeDirectEndpoints(
    NodeUtils::NodeIdType id,
    Configuration::TEndpointOptions const& options,
    IMessageSink* const messageSink,
    Configuration::OptionalPeersMap const& optBootstraps)
{
    auto const technology = Endpoints::TechnologyType::TCP;

    // Add the server based endpoint
    std::shared_ptr<CEndpoint> spServer = Endpoints::Factory(
        technology, id, options.GetInterface(), Endpoints::OperationType::Server, messageSink);

    spServer->ScheduleBind(options.GetBinding());

    m_endpoints.emplace(technology, spServer);

    // Add the client based endpoint
    std::shared_ptr<CEndpoint> spClient = Endpoints::Factory(
        technology, id, options.GetInterface(), Endpoints::OperationType::Client, messageSink);

    if (optBootstraps) {
        ConnectBootstraps(spClient, *optBootstraps);
    }

    m_endpoints.emplace(technology, spClient);

    m_technologies.emplace(technology);
}

//------------------------------------------------------------------------------------------------

void CEndpointManager::InitializeTCPEndpoints(
    NodeUtils::NodeIdType id,
    Configuration::TEndpointOptions const& options,
    IMessageSink* const messageSink,
    Configuration::OptionalPeersMap const& optBootstraps)
{
    auto const technology = Endpoints::TechnologyType::TCP;

    // Add the server based endpoint
    std::shared_ptr<CEndpoint> spServer = Endpoints::Factory(
        technology, id, options.GetInterface(), Endpoints::OperationType::Server, messageSink);

    spServer->ScheduleBind(options.GetBinding());

    m_endpoints.emplace(technology, spServer);

    // Add the client based endpoint
    std::shared_ptr<CEndpoint> spClient = Endpoints::Factory(
        technology, id, options.GetInterface(), Endpoints::OperationType::Client, messageSink);

    if (optBootstraps) {
        ConnectBootstraps(spClient, *optBootstraps);
    }

    m_endpoints.emplace(technology, spClient);

    m_technologies.emplace(technology);
}

//------------------------------------------------------------------------------------------------

void CEndpointManager::InitializeStreamBridgeEndpoints(
    NodeUtils::NodeIdType id,
    Configuration::TEndpointOptions const& options,
    IMessageSink* const messageSink)
{
    auto const technology = Endpoints::TechnologyType::StreamBridge;
    // Add the server based endpoint
    std::shared_ptr<CEndpoint> spServer = Endpoints::Factory(
        technology, id, options.GetInterface(), Endpoints::OperationType::Server, messageSink);

    spServer->ScheduleBind(options.GetBinding());

    m_endpoints.emplace(technology, spServer);

    m_technologies.emplace(technology);
}

//------------------------------------------------------------------------------------------------

template<typename FunctionType, typename...Args>
void CEndpointManager::NotifyObservers(FunctionType const& function, Args&&...args)
{
    std::scoped_lock lock(m_observersMutex);
    for (auto itr = m_observers.cbegin(); itr != m_observers.cend();) {
        auto const& observer = *itr; // Get a refernce to the current observer pointer
        // If the observer is no longer valid erase the dangling pointer from the set
        // Otherwise, send the observer the notification
        if (!observer) {
            itr = m_observers.erase(itr);
        } else {
            auto const binder = std::bind(function, observer, std::forward<Args>(args)...);
            binder();
            ++itr;
        }
    }
}

//------------------------------------------------------------------------------------------------

template<typename FunctionType, typename...Args>
void CEndpointManager::NotifyObserversConst(FunctionType const& function, Args&&...args) const
{
    std::scoped_lock lock(m_observersMutex);
    for (auto const& observer: m_observers) {
        auto const binder = std::bind(function, observer, std::forward<Args>(args)...);
        binder();
    }
}

//------------------------------------------------------------------------------------------------
