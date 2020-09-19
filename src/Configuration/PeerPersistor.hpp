//------------------------------------------------------------------------------------------------
// File: PeerPersistor.hpp
// Description:
//------------------------------------------------------------------------------------------------
#pragma once
//------------------------------------------------------------------------------------------------
#include "Configuration.hpp"
#include "StatusCode.hpp"
#include "../Components/Endpoints/ConnectionState.hpp"
#include "../Components/BryptPeer/BryptPeer.hpp"
#include "../Components/Endpoints/TechnologyType.hpp"
#include "../Interfaces/BootstrapCache.hpp"
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
#include <unordered_set>
//------------------------------------------------------------------------------------------------

namespace BryptIdentifier {
    class CContainer;
    using SharedContainer = std::shared_ptr<CContainer const>;
}

//------------------------------------------------------------------------------------------------

class CPeerPersistor : public IPeerObserver, public IBootstrapCache {
public:
    using BootstrapSet = std::unordered_set<std::string>;
    using UniqueBootstrapSet = std::unique_ptr<BootstrapSet>;

    using EndpointBootstrapMap = std::unordered_map<Endpoints::TechnologyType, UniqueBootstrapSet>;
    using UniqueEndpointBootstrapMap = std::unique_ptr<EndpointBootstrapMap>;

    using DefaultBootstrapMap = std::unordered_map<Endpoints::TechnologyType, std::string>;

    using BryptIdentifierSet = std::unordered_set<BryptIdentifier::SharedContainer>;
    using UniqueBryptIdentifierSet = std::unique_ptr<BryptIdentifierSet>;

    CPeerPersistor();
    explicit CPeerPersistor(std::string_view filepath);
    explicit CPeerPersistor(Configuration::EndpointConfigurations const& configurations);
    CPeerPersistor(
        std::string_view filepath,
        Configuration::EndpointConfigurations const& configurations);

    void SetMediator(IPeerMediator* const mediator);

    bool FetchBootstraps();
    Configuration::StatusCode Serialize();
    Configuration::StatusCode DecodePeersFile();
    Configuration::StatusCode SerializeEndpointPeers();
    Configuration::StatusCode SetupPeersFile();

    void AddBootstrapEntry(
        std::shared_ptr<CBryptPeer> const& spBryptPeer,
        Endpoints::EndpointIdType identifier,
        Endpoints::TechnologyType technology);
    void AddBootstrapEntry(
        Endpoints::TechnologyType technology,
        std::string_view bootstrap);
    void DeleteBootstrapEntry(
        std::shared_ptr<CBryptPeer> const& spBryptPeer,
        Endpoints::EndpointIdType identifier,
        Endpoints::TechnologyType technology);
    void DeleteBootstrapEntry(
        Endpoints::TechnologyType technology,
        std::string_view bootstrap);

    // IPeerObserver {
    void HandlePeerStateChange(
        std::weak_ptr<CBryptPeer> const& wpBryptPeer,
        Endpoints::EndpointIdType identifier,
        Endpoints::TechnologyType technology,
        ConnectionState change) override;
    // } IPeerObserver

    // IBootstrapCache {
    bool ForEachCachedBootstrap(
        AllEndpointBootstrapReadFunction const& callback,
        AllEndpointBootstrapErrorFunction const& error) const override;
    bool ForEachCachedBootstrap(
        Endpoints::TechnologyType technology,
        OneEndpointBootstrapReadFunction const& callback) const override;

    std::uint32_t CachedBootstrapCount() const override;
    std::uint32_t CachedBootstrapCount(Endpoints::TechnologyType technology) const override;
    // } IBootstrapCache
    
private:
    IPeerMediator* m_mediator;

    mutable std::mutex m_fileMutex;
    std::filesystem::path m_filepath;
    
    mutable std::mutex m_endpointBootstrapsMutex;
    UniqueEndpointBootstrapMap m_upEndpointBootstraps;

    DefaultBootstrapMap m_defaults;
};

//------------------------------------------------------------------------------------------------