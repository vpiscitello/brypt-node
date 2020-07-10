//------------------------------------------------------------------------------------------------
// File: PeerPersistor.hpp
// Description:
//------------------------------------------------------------------------------------------------
#pragma once
//------------------------------------------------------------------------------------------------
#include "StatusCode.hpp"
#include "../Components/Endpoints/ConnectionState.hpp"
#include "../Components/Endpoints/Peer.hpp"
#include "../Components/Endpoints/TechnologyType.hpp"
#include "../Interfaces/PeerCache.hpp"
#include "../Interfaces/PeerMediator.hpp"
#include "../Interfaces/PeerObserver.hpp"
#include "../Utilities/NodeUtils.hpp"
#include "../Utilities/CallbackIteration.hpp"
//------------------------------------------------------------------------------------------------
#include <filesystem>
#include <memory>
#include <mutex>
#include <string>
#include <string_view>
#include <unordered_map>
//------------------------------------------------------------------------------------------------

class CPeerPersistor : public IPeerCache, public IPeerObserver {
public:
    using PeersMap = std::unordered_map<std::string, CPeer>;
    using SharedPeersMap = std::shared_ptr<PeersMap>;

    using EndpointPeersMap = std::unordered_map<Endpoints::TechnologyType, SharedPeersMap>;
    using SharedEndpointPeersMap = std::shared_ptr<EndpointPeersMap>;

    CPeerPersistor();
    explicit CPeerPersistor(std::string_view filepath);

    void SetMediator(IPeerMediator* const mediator);

    bool FetchPeers();
    Configuration::StatusCode Serialize();
    Configuration::StatusCode DecodePeersFile();
    Configuration::StatusCode SerializeEndpointPeers();

    void AddPeerEntry(CPeer const& peer);
    void DeletePeerEntry(CPeer const& peer);

    // IPeerCache {
    bool ForEachCachedEndpoint(AllEndpointReadFunction const& readFunction) const override;
    bool ForEachCachedPeer(
        AllEndpointPeersReadFunction const& readFunction,
        AllEndpointPeersErrorFunction const& errorFunction) const override;
    bool ForEachCachedPeer(
        Endpoints::TechnologyType technology,
        OneEndpointPeersReadFunction const& readFunction) const override;

    std::uint32_t CachedEndpointsCount() const override;
    std::uint32_t CachedPeersCount() const override;
    std::uint32_t CachedPeersCount(Endpoints::TechnologyType technology) const override;
    // } IPeerCache

    // IPeerObserver {
    void HandlePeerConnectionStateChange(
        CPeer const& peer,
        ConnectionState change) override;
    // } IPeerObserver
    
private:
    IPeerMediator* m_mediator;
    std::filesystem::path m_filepath;
    mutable std::mutex m_fileMutex;
    mutable std::mutex m_endpointsMutex;
    SharedEndpointPeersMap m_spEndpoints;
};

//------------------------------------------------------------------------------------------------