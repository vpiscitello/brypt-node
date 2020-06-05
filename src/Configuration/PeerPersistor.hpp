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
#include "../Interfaces/PeerMediator.hpp"
#include "../Interfaces/PeerObserver.hpp"
#include "../Utilities/NodeUtils.hpp"
//------------------------------------------------------------------------------------------------
#include <filesystem>
#include <memory>
#include <mutex>
#include <string>
#include <string_view>
#include <unordered_map>
//------------------------------------------------------------------------------------------------

class CPeerPersistor : public IPeerObserver {
public:
    using PeersMap = std::unordered_map<NodeUtils::NodeIdType, CPeer>;
    using SharedPeersMap = std::shared_ptr<PeersMap>;

    using EndpointPeersMap = std::unordered_map<Endpoints::TechnologyType, SharedPeersMap>;
    using SharedEndpointPeersMap = std::shared_ptr<EndpointPeersMap>;

    CPeerPersistor();
    explicit CPeerPersistor(std::string_view filepath);

    void SetMediator(IPeerMediator* const mediator);

    SharedEndpointPeersMap FetchPeers();

    Configuration::StatusCode Serialize();
    Configuration::StatusCode DecodePeersFile();
    Configuration::StatusCode SerializeEndpointPeers();

    SharedEndpointPeersMap GetCachedPeers() const;
    SharedPeersMap GetCachedPeers(Endpoints::TechnologyType technology) const;

    // IPeerObserver {
    void HandlePeerConnectionStateChange(
        CPeer const& peer,
        ConnectionState change) override;
    // } IPeerObserver
    
private:
    IPeerMediator* m_mediator;
    std::filesystem::path m_filepath;
    std::mutex m_fileMutex;
    std::mutex m_endpointsMutex;
    SharedEndpointPeersMap m_spEndpoints;
};

//------------------------------------------------------------------------------------------------