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
    CPeerPersistor::SharedEndpointPeersMap const& spEndpointBootstraps)
{
    // Iterate through the provided configurations to setup the endpoints for the given technolgy
    for (auto const& options: configurations) {
        auto const technology = options.GetTechnology();
        // If the technology has not already been configured then continue with the setup. 
        // This function should only be called once per application run, there shouldn't be a reason
        // to re-initilize a technology as the endpoints should exist until appliction termination.
        if (auto const itr = m_technologies.find(technology); itr == m_technologies.end()) {
            // Attempt to find bootstrap peers in the provided endpoint bootstraps. If the technology
            // has bootstraps available forward it to the function during technology initialization.
            CPeerPersistor::SharedPeersMap spBootstraps;
            if (spEndpointBootstraps) {
                if (auto const itr = spEndpointBootstraps->find(technology); itr != spEndpointBootstraps->end()) {
                    spBootstraps = itr->second;
                }
            }            
            switch (technology) {
                case Endpoints::TechnologyType::Direct: {
                    InitializeDirectEndpoints(id, options, messageSink, spBootstraps);
                } break;
                case Endpoints::TechnologyType::TCP: {
                    InitializeTCPEndpoints(id, options, messageSink, spBootstraps);
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
    CPeerPersistor::PeersMap const& bootstraps)
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
    for (auto const& [identifier, spEndpoint]: m_endpoints) {
        spEndpoint->Startup();
    }
}

//------------------------------------------------------------------------------------------------

void CEndpointManager::Shutdown()
{
    for (auto const& [identifier, spEndpoint]: m_endpoints) {
        if (spEndpoint) {
            [[maybe_unused]] bool const bStopped = spEndpoint->Shutdown();
        }
    }
}

//------------------------------------------------------------------------------------------------

CEndpointManager::SharedEndpoint CEndpointManager::GetEndpoint(Endpoints::EndpointIdType identifier) const
{
    if (auto const itr = m_endpoints.find(identifier); itr != m_endpoints.end()) {
        return itr->second;
    }
    return nullptr;
}

//------------------------------------------------------------------------------------------------

CEndpointManager::SharedEndpoint CEndpointManager::GetEndpoint(
    Endpoints::TechnologyType technology,
    Endpoints::OperationType operation) const
{
    for (auto const [identifier, spEndpoint]: m_endpoints) {
        if (technology == spEndpoint->GetInternalType() && operation == spEndpoint->GetOperation()) {
            return spEndpoint;
        }
    }
    return nullptr;
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
    for (auto const& [identifier, spEndpoint] : m_endpoints) {
        if (spEndpoint && spEndpoint->IsActive()) {
            ++count;
        }
    }
    return count;
}

//------------------------------------------------------------------------------------------------

std::uint32_t CEndpointManager::ActiveTechnologyCount() const
{
    Endpoints::TechnologySet technologies;
    for (auto const& [identifier, spEndpoint] : m_endpoints) {
        if (spEndpoint && spEndpoint->IsActive()) {
            technologies.emplace(spEndpoint->GetInternalType());
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
    CPeerPersistor::SharedPeersMap const& spBootstraps)
{
    auto const technology = Endpoints::TechnologyType::Direct;

    // Add the server based endpoint
    std::shared_ptr<CEndpoint> spServer = Endpoints::Factory(
        technology, id, options.GetInterface(), Endpoints::OperationType::Server, messageSink);

    spServer->ScheduleBind(options.GetBinding());

    m_endpoints.emplace(spServer->GetIdentifier(), spServer);

    // Add the client based endpoint
    std::shared_ptr<CEndpoint> spClient = Endpoints::Factory(
        technology, id, options.GetInterface(), Endpoints::OperationType::Client, messageSink);

    if (spBootstraps) {
        ConnectBootstraps(spClient, *spBootstraps);
    }

    m_endpoints.emplace(spClient->GetIdentifier(), spClient);

    m_technologies.emplace(technology);
}

//------------------------------------------------------------------------------------------------

void CEndpointManager::InitializeTCPEndpoints(
    NodeUtils::NodeIdType id,
    Configuration::TEndpointOptions const& options,
    IMessageSink* const messageSink,
    CPeerPersistor::SharedPeersMap const& spBootstraps)
{
    auto const technology = Endpoints::TechnologyType::TCP;

    // Add the server based endpoint
    std::shared_ptr<CEndpoint> spServer = Endpoints::Factory(
        technology, id, options.GetInterface(), Endpoints::OperationType::Server, messageSink);

    spServer->ScheduleBind(options.GetBinding());

    m_endpoints.emplace(spServer->GetIdentifier(), spServer);

    // Add the client based endpoint
    std::shared_ptr<CEndpoint> spClient = Endpoints::Factory(
        technology, id, options.GetInterface(), Endpoints::OperationType::Client, messageSink);

    if (spBootstraps) {
        ConnectBootstraps(spClient, *spBootstraps);
    }

    m_endpoints.emplace(spClient->GetIdentifier(), spClient);

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

    m_endpoints.emplace(spServer->GetIdentifier(), spServer);

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
