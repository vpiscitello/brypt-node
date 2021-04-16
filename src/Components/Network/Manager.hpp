//----------------------------------------------------------------------------------------------------------------------
// File: Manager.hpp
// Description: 
//----------------------------------------------------------------------------------------------------------------------
#pragma once
//----------------------------------------------------------------------------------------------------------------------
#include "EndpointIdentifier.hpp"
#include "EndpointTypes.hpp"
#include "Protocol.hpp"
#include "Components/Configuration/Configuration.hpp"
#include "Components/Configuration/PeerPersistor.hpp"
#include "Components/Peer/Manager.hpp"
#include "Interfaces/EndpointMediator.hpp"
#include "Interfaces/MessageSink.hpp"
#include "Interfaces/PeerMediator.hpp"
#include "Interfaces/PeerObserver.hpp"
#include "Utilities/NodeUtils.hpp"
//----------------------------------------------------------------------------------------------------------------------
#include <memory>
#include <mutex>
#include <set>
#include <unordered_map>
//----------------------------------------------------------------------------------------------------------------------

//----------------------------------------------------------------------------------------------------------------------
namespace Network {
//----------------------------------------------------------------------------------------------------------------------

class IEndpoint;
class Manager;

//----------------------------------------------------------------------------------------------------------------------
}
//----------------------------------------------------------------------------------------------------------------------

class Network::Manager final : public IEndpointMediator
{
public:
    using SharedEndpoint = std::shared_ptr<Network::IEndpoint>;

    Manager(
        Configuration::EndpointConfigurations const& configurations,
        IPeerMediator* const pPeerMediator,
        IBootstrapCache const* const pBootstrapCache);

    Manager(Manager const& other) = delete;
    Manager& operator=(Manager const& other) = delete;

    ~Manager();

    // IEndpointMediator {
    [[nodiscard]] virtual EndpointEntryMap GetEndpointEntries() const override;
    [[nodiscard]] virtual EndpointUriSet GetEndpointUris() const override;
    // } IEndpointMediator
    
    void Startup();
    void Shutdown();

    [[nodiscard]] SharedEndpoint GetEndpoint(Network::Endpoint::Identifier identifier) const;
    [[nodiscard]] SharedEndpoint GetEndpoint(Network::Protocol protocol, Network::Operation operation) const;
    [[nodiscard]] Network::ProtocolSet GetEndpointProtocols() const;
    [[nodiscard]] std::size_t ActiveEndpointCount() const;
    [[nodiscard]] std::size_t ActiveProtocolCount() const;

private:
    using EndpointsMap = std::unordered_map<Network::Endpoint::Identifier, SharedEndpoint>;

    void Initialize(
        Configuration::EndpointConfigurations const& configurations,
        IPeerMediator* const pPeerMediator,
        IBootstrapCache const* const pBootstrapCache);
        
    void InitializeTCPEndpoints(
        Configuration::EndpointOptions const& options,
        IPeerMediator* const pPeerMediator,
        IBootstrapCache const* const pBootstrapCache);

    EndpointsMap m_endpoints;
    Network::ProtocolSet m_protocols;
};

//----------------------------------------------------------------------------------------------------------------------
