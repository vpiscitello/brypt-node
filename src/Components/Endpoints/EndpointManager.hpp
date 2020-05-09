//------------------------------------------------------------------------------------------------
// File: Endpoint.hpp
// Description: Defines a set of communication methods for use on varying types of communication
// technologies. Currently supports ZMQ D, ZMQ StreamBridge, and TCP sockets.
//------------------------------------------------------------------------------------------------
#pragma once
//------------------------------------------------------------------------------------------------
#include "EndpointTypes.hpp"
#include "../../Configuration/Configuration.hpp"
#include "../../Configuration/PeerPersistor.hpp"
#include "../../Interfaces/MessageSink.hpp"
#include "../../Interfaces/PeerMediator.hpp"
#include "../../Interfaces/PeerObserver.hpp"
#include "../../Utilities/NodeUtils.hpp"
//------------------------------------------------------------------------------------------------
#include <mutex>
#include <set>
//------------------------------------------------------------------------------------------------

class CEndpoint;

//------------------------------------------------------------------------------------------------

class CEndpointManager : public IPeerMediator {
public:
    using ObserverSet = std::set<IPeerObserver*>;

    CEndpointManager();

    CEndpointManager(CEndpointManager const& other) = delete;
    CEndpointManager& operator=(CEndpointManager const& other) = delete;

    ~CEndpointManager();

    void Initialize(
        NodeUtils::NodeIdType id,
        IMessageSink* const messageSink,
        Configuration::EndpointConfigurations const& configurations,
        Configuration::OptionalEndpointPeersMap const& optEndpointBootstraps);
    void ConnectBootstraps(
        std::shared_ptr<CEndpoint> const& spEndpoint,
        Configuration::PeersMap const& peers);
    void Startup();
    void Shutdown();

    Endpoints::TechnologySet GetEndpointTechnologies() const;
    std::uint32_t ActiveEndpointCount() const;
    std::uint32_t ActiveTechnologyCount() const;

    // IPeerMediator {
    virtual void RegisterObserver(IPeerObserver* const observer) override;
    virtual void UnpublishObserver(IPeerObserver* const observer) override;

    virtual void ForwardPeerConnectionStateChange(
        CPeer const& peer, ConnectionState change) override;
    // } IPeerMediator
    
private:
    using EndpointsMultimap = std::unordered_multimap<Endpoints::TechnologyType, std::shared_ptr<CEndpoint>>;

    void InitializeDirectEndpoints(
        NodeUtils::NodeIdType id,
        Configuration::TEndpointOptions const& options,
        IMessageSink* const messageSink,
        Configuration::OptionalPeersMap const& optBootstraps);
    void InitializeTCPEndpoints(
        NodeUtils::NodeIdType id,
        Configuration::TEndpointOptions const& options,
        IMessageSink* const messageSink,
        Configuration::OptionalPeersMap const& optBootstraps);
    void InitializeStreamBridgeEndpoints(
        NodeUtils::NodeIdType id,
        Configuration::TEndpointOptions const& options,
        IMessageSink* const messageSink);

    template<typename FunctionType, typename...Args>
    void NotifyObservers(FunctionType const& function, Args&&...args);

    template<typename FunctionType, typename...Args>
    void NotifyObserversConst(FunctionType const& function, Args&&...args) const;

    EndpointsMultimap m_endpoints;
    Endpoints::TechnologySet m_technologies;

    mutable std::mutex m_observersMutex;
    ObserverSet m_observers;
};

//------------------------------------------------------------------------------------------------
