//------------------------------------------------------------------------------------------------
// File: PeerPersistor.hpp
// Description:
//------------------------------------------------------------------------------------------------
#pragma once
//------------------------------------------------------------------------------------------------
#include "Configuration.hpp"
#include "StatusCode.hpp"
#include "BryptIdentifier/IdentifierTypes.hpp"
#include "Components/Network/ConnectionState.hpp"
#include "Components/BryptPeer/BryptPeer.hpp"
#include "Components/Network/Protocol.hpp"
#include "Interfaces/BootstrapCache.hpp"
#include "Interfaces/PeerMediator.hpp"
#include "Interfaces/PeerObserver.hpp"
#include "Utilities/CallbackIteration.hpp"
//------------------------------------------------------------------------------------------------
#include <filesystem>
#include <memory>
#include <mutex>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
//------------------------------------------------------------------------------------------------

namespace spdlog { class logger; }

//------------------------------------------------------------------------------------------------

class PeerPersistor : public IPeerObserver, public IBootstrapCache {
public:
    using BootstrapSet = std::unordered_set<std::string>;
    using UniqueBootstrapSet = std::unique_ptr<BootstrapSet>;

    using ProtocolMap = std::unordered_map<Network::Protocol, UniqueBootstrapSet>;
    using UniqueProtocolMap = std::unique_ptr<ProtocolMap>;

    using DefaultBootstrapMap = std::unordered_map<Network::Protocol, std::string>;

    using BryptIdentifierSet = std::unordered_set<BryptIdentifier::SharedContainer>;
    using UniqueBryptIdentifierSet = std::unique_ptr<BryptIdentifierSet>;

    PeerPersistor();
    explicit PeerPersistor(std::string_view filepath);
    explicit PeerPersistor(Configuration::EndpointConfigurations const& configurations);
    PeerPersistor(
        std::string_view filepath,
        Configuration::EndpointConfigurations const& configurations);

    void SetMediator(IPeerMediator* const mediator);

    bool FetchBootstraps();
    Configuration::StatusCode Serialize();
    Configuration::StatusCode DecodePeersFile();
    Configuration::StatusCode SerializeEndpointPeers();
    Configuration::StatusCode SetupPeersFile();

    void AddBootstrapEntry(
        std::shared_ptr<BryptPeer> const& spBryptPeer,
        Network::Endpoint::Identifier identifier,
        Network::Protocol protocol);
    void AddBootstrapEntry(
        Network::Protocol protocol,
        std::string_view bootstrap);
    void DeleteBootstrapEntry(
        std::shared_ptr<BryptPeer> const& spBryptPeer,
        Network::Endpoint::Identifier identifier,
        Network::Protocol protocol);
    void DeleteBootstrapEntry(
        Network::Protocol protocol,
        std::string_view bootstrap);

    // IPeerObserver {
    void HandlePeerStateChange(
        std::weak_ptr<BryptPeer> const& wpBryptPeer,
        Network::Endpoint::Identifier identifier,
        Network::Protocol protocol,
        ConnectionState change) override;
    // } IPeerObserver

    // IBootstrapCache {
    bool ForEachCachedBootstrap(
        AllProtocolsReadFunction const& callback,
        AllProtocolsErrorFunction const& error) const override;
    bool ForEachCachedBootstrap(
        Network::Protocol protocol,
        OneProtocolReadFunction const& callback) const override;

    std::size_t CachedBootstrapCount() const override;
    std::size_t CachedBootstrapCount(Network::Protocol protocol) const override;
    // } IBootstrapCache
    
private:
    std::shared_ptr<spdlog::logger> m_spLogger;
    
    IPeerMediator* m_mediator;

    mutable std::mutex m_fileMutex;
    std::filesystem::path m_filepath;
    
    mutable std::mutex m_protocolsMutex;
    UniqueProtocolMap m_upProtocols;

    DefaultBootstrapMap m_defaults;
};

//------------------------------------------------------------------------------------------------