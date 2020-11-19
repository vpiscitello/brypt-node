//------------------------------------------------------------------------------------------------
// File: EndpointManager.cpp
// Description: 
//------------------------------------------------------------------------------------------------
#include "EndpointManager.hpp"
#include "Endpoint.hpp"
//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
namespace {
namespace local {
//------------------------------------------------------------------------------------------------

void ConnectBootstraps(
    std::shared_ptr<CEndpoint> const& spEndpoint,
    IBootstrapCache const* const pBootstrapCache);

//------------------------------------------------------------------------------------------------
} // local namespace
} // namespace
//------------------------------------------------------------------------------------------------

CEndpointManager::CEndpointManager(
    Configuration::EndpointConfigurations const& configurations,
    IPeerMediator* const pPeerMediator,
    IBootstrapCache const* const pBootstrapCache)
    : m_endpoints()
    , m_technologies()
{
    Initialize(configurations, pPeerMediator, pBootstrapCache);
}

//------------------------------------------------------------------------------------------------

CEndpointManager::~CEndpointManager()
{
    Shutdown();
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

IEndpointMediator::EndpointEntryMap CEndpointManager::GetEndpointEntries() const
{
    EndpointEntryMap entries;
    for (auto const& [identifier, spEndpoint]: m_endpoints) {
        if (spEndpoint) {
            if (auto const entry = spEndpoint->GetEntry(); !entry.empty()) {
                entries.emplace(spEndpoint->GetInternalType(), entry);
            }
        }
    }
    return entries;
}

//------------------------------------------------------------------------------------------------

IEndpointMediator::EndpointURISet CEndpointManager::GetEndpointURIs() const
{
    EndpointURISet uris;
    for (auto const& [identifier, spEndpoint]: m_endpoints) {
        if (spEndpoint) {
            if (auto const uri = spEndpoint->GetURI(); !uri.empty()) {
                uris.emplace(uri);
            }
        }
    }
    return uris;
}

//------------------------------------------------------------------------------------------------

void CEndpointManager::Initialize(
    Configuration::EndpointConfigurations const& configurations,
    IPeerMediator* const pPeerMediator,
    IBootstrapCache const* const pBootstrapCache)
{
    // Iterate through the provided configurations to setup the endpoints for the given technolgy
    for (auto const& options: configurations) {
        auto const technology = options.GetTechnology();
        // If the technology has not already been configured then continue with the setup. 
        // This function should only be called once per application run, there shouldn't be a reason
        // to re-initilize a technology as the endpoints should exist until appliction termination.
        if (auto const itr = m_technologies.find(technology); itr == m_technologies.end()) {         
            switch (technology) {
                case Endpoints::TechnologyType::TCP: {
                    InitializeTCPEndpoints(options, pPeerMediator, pBootstrapCache);
                } break;
                default: break; // No other technologies have implemented endpoints
            }
        }
    }
}

//------------------------------------------------------------------------------------------------

void CEndpointManager::InitializeTCPEndpoints(
    Configuration::TEndpointOptions const& options,
    IPeerMediator* const pPeerMediator,
    IBootstrapCache const* const pBootstrapCache)
{
    assert(options.GetTechnology() == Endpoints::TechnologyType::TCP);

    // Add the server based endpoint
    std::shared_ptr<CEndpoint> spServer = Endpoints::Factory(
        options.GetTechnology(), options.GetInterface(),
        Endpoints::OperationType::Server, this, pPeerMediator);

    spServer->ScheduleBind(options.GetBinding());

    m_endpoints.emplace(spServer->GetEndpointIdentifier(), spServer);

    // Add the client based endpoint
    std::shared_ptr<CEndpoint> spClient = Endpoints::Factory(
        options.GetTechnology(), options.GetInterface(),
        Endpoints::OperationType::Client, this, pPeerMediator);

    if (pBootstrapCache) {
        local::ConnectBootstraps(spClient, pBootstrapCache);
    }

    m_endpoints.emplace(spClient->GetEndpointIdentifier(), spClient);

    m_technologies.emplace(options.GetTechnology());
}

//------------------------------------------------------------------------------------------------

void local::ConnectBootstraps(
    std::shared_ptr<CEndpoint> const& spEndpoint,
    IBootstrapCache const* const pBootstrapCache)
{
    // If the given endpoint id not able to connect to peers, don't do anything.
    if (!spEndpoint || spEndpoint->GetOperation() != Endpoints::OperationType::Client) {
        return;
    }

    if (!pBootstrapCache) {
        return;
    }

    // Iterate through the provided bootstraps for the endpoint and schedule a connect
    // for each peer in the list.
    pBootstrapCache->ForEachCachedBootstrap(
        spEndpoint->GetInternalType(), 
        [&spEndpoint] (std::string_view const& bootstrap) -> CallbackIteration {
            spEndpoint->ScheduleConnect(bootstrap);
            return CallbackIteration::Continue;
        });
}

//------------------------------------------------------------------------------------------------
