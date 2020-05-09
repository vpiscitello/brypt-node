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
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
namespace Configuration {
//------------------------------------------------------------------------------------------------

class CPeerPersistor;

using PeersMap = std::unordered_map<NodeUtils::NodeIdType, CPeer>;
using OptionalPeersMap = std::optional<PeersMap>;

using EndpointPeersMap = std::unordered_map<Endpoints::TechnologyType, PeersMap>;
using OptionalEndpointPeersMap = std::optional<EndpointPeersMap>;

//------------------------------------------------------------------------------------------------
} // Configuration namespace
//------------------------------------------------------------------------------------------------

class Configuration::CPeerPersistor : public IPeerObserver {
public:
    CPeerPersistor();
    explicit CPeerPersistor(std::string_view filepath);

    void SetMediator(IPeerMediator* const mediator);

    OptionalEndpointPeersMap FetchPeers();

    StatusCode Serialize();
    StatusCode DecodePeersFile();
    StatusCode SerializeEndpointPeers();

    OptionalEndpointPeersMap GetCachedPeers() const;
    OptionalPeersMap GetCachedPeers(Endpoints::TechnologyType technology) const;

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
    EndpointPeersMap m_endpoints;
};

//------------------------------------------------------------------------------------------------