//------------------------------------------------------------------------------------------------
// File: EndpointManager.hpp
// Description: 
//------------------------------------------------------------------------------------------------
#pragma once
//------------------------------------------------------------------------------------------------
#include "EndpointTypes.hpp"
#include "EndpointIdentifier.hpp"
#include "../../Configuration/Configuration.hpp"
#include "../../Configuration/PeerPersistor.hpp"
#include "../../Interfaces/EndpointMediator.hpp"
#include "../../Interfaces/MessageSink.hpp"
#include "../../Interfaces/PeerMediator.hpp"
#include "../../Interfaces/PeerObserver.hpp"
#include "../../Utilities/NodeUtils.hpp"
//------------------------------------------------------------------------------------------------
#include <mutex>
#include <set>
#include <unordered_map>
//------------------------------------------------------------------------------------------------

class CEndpoint;

//------------------------------------------------------------------------------------------------

class CEndpointManager : public IEndpointMediator {
public:
    using SharedEndpoint = std::shared_ptr<CEndpoint>;

    CEndpointManager();

    CEndpointManager(CEndpointManager const& other) = delete;
    CEndpointManager& operator=(CEndpointManager const& other) = delete;

    ~CEndpointManager();

    void Initialize(
        BryptIdentifier::SharedContainer const& spBryptIdentifier,
        IPeerMediator* const pPeerMediator,
        IMessageSink* const pMessageSink,
        Configuration::EndpointConfigurations const& configurations,
        IBootstrapCache const* const pBootsrapCache);
    void Startup();
    void Shutdown();

    SharedEndpoint GetEndpoint(Endpoints::EndpointIdType identifier) const;
    SharedEndpoint GetEndpoint(
        Endpoints::TechnologyType technology,
        Endpoints::OperationType operation) const;
    Endpoints::TechnologySet GetEndpointTechnologies() const;
    std::uint32_t ActiveEndpointCount() const;
    std::uint32_t ActiveTechnologyCount() const;

    // IEndpointMediator {
    virtual EndpointEntryMap GetEndpointEntries() const override;
    virtual EndpointURISet GetEndpointURIs() const override;
    // } IEndpointMediator
    
private:
    using EndpointsMultimap = std::unordered_map<Endpoints::EndpointIdType, SharedEndpoint>;

    void InitializeTCPEndpoints(
        BryptIdentifier::SharedContainer const& spBryptIdentifier,
        Configuration::TEndpointOptions const& options,
        IPeerMediator* const pPeerMediator,
        IBootstrapCache const* const pBootstrapCache);

    EndpointsMultimap m_endpoints;
    Endpoints::TechnologySet m_technologies;

};

//------------------------------------------------------------------------------------------------
