//----------------------------------------------------------------------------------------------------------------------
// File: PeerPersistor.hpp
// Description:
//----------------------------------------------------------------------------------------------------------------------
#pragma once
//----------------------------------------------------------------------------------------------------------------------
#include "Configuration.hpp"
#include "StatusCode.hpp"
#include "BryptIdentifier/IdentifierTypes.hpp"
#include "Components/BryptPeer/BryptPeer.hpp"
#include "Components/Network/Address.hpp"
#include "Components/Network/ConnectionState.hpp"
#include "Components/Network/Protocol.hpp"
#include "Interfaces/BootstrapCache.hpp"
#include "Interfaces/PeerMediator.hpp"
#include "Interfaces/PeerObserver.hpp"
#include "Utilities/CallbackIteration.hpp"
//----------------------------------------------------------------------------------------------------------------------
#include <filesystem>
#include <memory>
#include <mutex>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
//----------------------------------------------------------------------------------------------------------------------

namespace spdlog { class logger; }

//----------------------------------------------------------------------------------------------------------------------

class PeerPersistor : public IPeerObserver, public IBootstrapCache {
public:
    using BootstrapSet = std::unordered_set<
        Network::RemoteAddress, Network::AddressHasher<Network::RemoteAddress>>;
    using UniqueBootstrapSet = std::unique_ptr<BootstrapSet>;

    using ProtocolMap = std::unordered_map<Network::Protocol, UniqueBootstrapSet>;
    using UniqueProtocolMap = std::unique_ptr<ProtocolMap>;

    using DefaultBootstrapMap = std::unordered_map<
        Network::Protocol, std::optional<Network::RemoteAddress>>;

    explicit PeerPersistor(
        std::filesystem::path const& filepath,
        Configuration::EndpointConfigurations const& configurations = {},
        bool shouldBuildPath = true);

    void SetMediator(IPeerMediator* const mediator);

    bool FetchBootstraps();
    Configuration::StatusCode Serialize();
    Configuration::StatusCode DecodePeersFile();
    Configuration::StatusCode SerializeEndpointPeers();
    Configuration::StatusCode SetupPeersFile();

    void AddBootstrapEntry(
        std::shared_ptr<BryptPeer> const& spBryptPeer,
        Network::Endpoint::Identifier identifier);
    void AddBootstrapEntry(Network::RemoteAddress const& bootstrap);
    void DeleteBootstrapEntry(
        std::shared_ptr<BryptPeer> const& spBryptPeer,
        Network::Endpoint::Identifier identifier);
    void DeleteBootstrapEntry(Network::RemoteAddress const& bootstrap);

    // IPeerObserver {
    virtual void HandlePeerStateChange(
        std::weak_ptr<BryptPeer> const& wpBryptPeer,
        Network::Endpoint::Identifier identifier,
        Network::Protocol protocol,
        ConnectionState change) override;
    // } IPeerObserver

    // IBootstrapCache {
    virtual bool ForEachCachedBootstrap(
        AllProtocolsReadFunction const& callback,
        AllProtocolsErrorFunction const& error) const override;
    virtual bool ForEachCachedBootstrap(
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

//----------------------------------------------------------------------------------------------------------------------