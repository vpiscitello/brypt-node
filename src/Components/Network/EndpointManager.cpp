//------------------------------------------------------------------------------------------------
// File: EndpointManager.cpp
// Description: 
//------------------------------------------------------------------------------------------------
#include "Endpoint.hpp"
#include "EndpointManager.hpp"
#include "Components/Network/LoRa/Endpoint.hpp"
#include "Components/Network/TCP/Endpoint.hpp"
//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
namespace {
namespace local {
//------------------------------------------------------------------------------------------------

void ConnectBootstraps(
    EndpointManager::SharedEndpoint const& spEndpoint, IBootstrapCache const* const pCache);

//------------------------------------------------------------------------------------------------
} // local namespace
} // namespace
//------------------------------------------------------------------------------------------------

EndpointManager::EndpointManager(
    Configuration::EndpointConfigurations const& configurations,
    IPeerMediator* const pPeerMediator,
    IBootstrapCache const* const pBootstrapCache)
    : m_endpoints()
    , m_protocols()
{
    Initialize(configurations, pPeerMediator, pBootstrapCache);
}

//------------------------------------------------------------------------------------------------

EndpointManager::~EndpointManager()
{
    Shutdown();
}

//------------------------------------------------------------------------------------------------

EndpointManager::EndpointEntryMap EndpointManager::GetEndpointEntries() const
{
    EndpointEntryMap entries;
    for (auto const& [identifier, spEndpoint]: m_endpoints) {
        if (spEndpoint) {
            if (auto const binding = spEndpoint->GetBinding(); binding.IsValid()) {
                entries.emplace(spEndpoint->GetProtocol(), binding.GetAuthority());
            }
        }
    }
    return entries;
}

//------------------------------------------------------------------------------------------------

EndpointManager::EndpointUriSet EndpointManager::GetEndpointUris() const
{
    EndpointUriSet uris;
    for (auto const& [identifier, spEndpoint]: m_endpoints) {
        if (spEndpoint) {
            if (auto const binding = spEndpoint->GetBinding(); binding.IsValid()) {
                uris.emplace(binding.GetUri());
            }
        }
    }
    return uris;
}

//------------------------------------------------------------------------------------------------

void EndpointManager::Startup()
{
    for (auto const& [identifier, spEndpoint]: m_endpoints) {
        spEndpoint->Startup();
    }
}

//------------------------------------------------------------------------------------------------

void EndpointManager::Shutdown()
{
    for (auto const& [identifier, spEndpoint]: m_endpoints) {
        if (spEndpoint) {
            [[maybe_unused]] bool const bStopped = spEndpoint->Shutdown();
        }
    }
}

//------------------------------------------------------------------------------------------------

EndpointManager::SharedEndpoint EndpointManager::GetEndpoint(
    Network::Endpoint::Identifier identifier) const
{
    if (auto const itr = m_endpoints.find(identifier); itr != m_endpoints.end()) {
        return itr->second;
    }
    return nullptr;
}

//------------------------------------------------------------------------------------------------

EndpointManager::SharedEndpoint EndpointManager::GetEndpoint(
    Network::Protocol protocol,
    Network::Operation operation) const
{
    for (auto const [identifier, spEndpoint]: m_endpoints) {
        if (protocol == spEndpoint->GetProtocol() && operation == spEndpoint->GetOperation()) {
            return spEndpoint;
        }
    }
    return nullptr;
}

//------------------------------------------------------------------------------------------------

Network::ProtocolSet EndpointManager::GetEndpointProtocols() const
{
    return m_protocols;
}

//------------------------------------------------------------------------------------------------

std::size_t EndpointManager::ActiveEndpointCount() const
{
    std::uint32_t count = 0;
    for (auto const& [identifier, spEndpoint] : m_endpoints) {
        if (spEndpoint && spEndpoint->IsActive()) { ++count; }
    }
    return count;
}

//------------------------------------------------------------------------------------------------

std::size_t EndpointManager::ActiveProtocolCount() const
{
    Network::ProtocolSet protocols;
    for (auto const& [identifier, spEndpoint] : m_endpoints) {
        if (spEndpoint && spEndpoint->IsActive()) {
            protocols.emplace(spEndpoint->GetProtocol());
        }
    }
    return protocols.size();
}

//------------------------------------------------------------------------------------------------

void EndpointManager::Initialize(
    Configuration::EndpointConfigurations const& configurations,
    IPeerMediator* const pPeerMediator,
    IBootstrapCache const* const pBootstrapCache)
{
    // Iterate through the provided configurations to setup the endpoints for the given technolgy
    for (auto const& options: configurations) {
        auto const protocol = options.GetProtocol();
        // If the protocol has not already been configured then continue with the setup. 
        // This function should only be called once per application run, there shouldn't be a reason
        // to re-initilize a protocol as the endpoints should exist until appliction termination.
        if (auto const itr = m_protocols.find(protocol); itr == m_protocols.end()) {         
            switch (protocol) {
                case Network::Protocol::TCP: {
                    InitializeTCPEndpoints(options, pPeerMediator, pBootstrapCache);
                } break;
                default: break; // No other protocols have implemented endpoints
            }
        }
    }
}

//------------------------------------------------------------------------------------------------

void EndpointManager::InitializeTCPEndpoints(
    Configuration::EndpointOptions const& options,
    IPeerMediator* const pPeerMediator,
    IBootstrapCache const* const pBootstrapCache)
{
    assert(options.GetProtocol() == Network::Protocol::TCP);

    // Add the server based endpoint
    SharedEndpoint spServer = Network::Endpoint::Factory(
        options.GetProtocol(), Network::Operation::Server, this, pPeerMediator);

    spServer->ScheduleBind(options.GetBinding());

    m_endpoints.emplace(spServer->GetEndpointIdentifier(), spServer);

    // Add the client based endpoint
    SharedEndpoint spClient = Network::Endpoint::Factory(
        options.GetProtocol(), Network::Operation::Client, this, pPeerMediator);

    if (pBootstrapCache) {
        local::ConnectBootstraps(spClient, pBootstrapCache);
    }

    m_endpoints.emplace(spClient->GetEndpointIdentifier(), spClient);

    m_protocols.emplace(options.GetProtocol());
}

//------------------------------------------------------------------------------------------------

void local::ConnectBootstraps(
    EndpointManager::SharedEndpoint const& spEndpoint, IBootstrapCache const* const pCache)
{
    using namespace Network;
    assert(spEndpoint && spEndpoint->GetOperation() == Operation::Client);
    assert(pCache);

    // Iterate through the provided bootstraps for the endpoint and schedule a connect
    // for each peer in the list.
    pCache->ForEachCachedBootstrap(
        spEndpoint->GetProtocol(), 
        [&spEndpoint] (std::string_view const& bootstrap) -> CallbackIteration
        {
            spEndpoint->ScheduleConnect(bootstrap);
            return CallbackIteration::Continue;
        });
}

//------------------------------------------------------------------------------------------------
