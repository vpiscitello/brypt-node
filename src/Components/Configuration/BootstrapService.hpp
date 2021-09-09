//----------------------------------------------------------------------------------------------------------------------
// File: BootstrapService.hpp
// Description:
//----------------------------------------------------------------------------------------------------------------------
#pragma once
//----------------------------------------------------------------------------------------------------------------------
#include "Options.hpp"
#include "StatusCode.hpp"
#include "Components/Network/Address.hpp"
#include "Components/Network/Protocol.hpp"
#include "Interfaces/BootstrapCache.hpp"
#include "Interfaces/PeerMediator.hpp"
#include "Interfaces/PeerObserver.hpp"
#include "Utilities/CallbackIteration.hpp"
//----------------------------------------------------------------------------------------------------------------------
#include <filesystem>
#include <memory>
#include <mutex>
#include <unordered_map>
#include <unordered_set>
#include <vector>
//----------------------------------------------------------------------------------------------------------------------

namespace spdlog { class logger; }

namespace Scheduler {
    class Delegate;
    class Service;
}

//----------------------------------------------------------------------------------------------------------------------

class BootstrapService : public IPeerObserver, public IBootstrapCache
{
public:
    using BootstrapCache = std::unordered_set<Network::RemoteAddress, Network::AddressHasher<Network::RemoteAddress>>;
    using DefaultBootstraps = std::unordered_map<Network::Protocol, std::optional<Network::RemoteAddress>>;

    struct CacheUpdateResult { std::size_t applied; std::int32_t difference; };

    BootstrapService();
    explicit BootstrapService(std::filesystem::path const& filepath, bool useFilepathDeduction = true);
    ~BootstrapService();

    std::filesystem::path const& GetFilepath() const;
    void SetFilepath(std::filesystem::path const& filepath);
    void DisableFilesystem();
    [[nodiscard]] bool FilesystemDisabled() const;

    void SetDefaults(Configuration::Options::Endpoints const& endpoints);

    void Register(IPeerMediator* const mediator);
    void Register(std::shared_ptr<Scheduler::Service> const& spScheduler);
    void UnregisterServices();

    [[nodiscard]] bool FetchBootstraps();
    void InsertBootstrap(Network::RemoteAddress const& bootstrap);
    void RemoveBootstrap(Network::RemoteAddress const& bootstrap);
    CacheUpdateResult UpdateCache();
    [[nodiscard]] Configuration::StatusCode Serialize();

    // IPeerObserver {
    virtual void OnRemoteConnected(Network::Endpoint::Identifier, Network::RemoteAddress const& address) override;
    virtual void OnRemoteDisconnected(Network::Endpoint::Identifier, Network::RemoteAddress const& address) override;
    // } IPeerObserver

    // IBootstrapCache {
    [[nodiscard]] virtual bool Contains(Network::RemoteAddress const& address) const override;

    virtual std::size_t ForEachBootstrap(BootstrapReader const& reader) const override;
    virtual std::size_t ForEachBootstrap(Network::Protocol protocol, BootstrapReader const& reader) const override;

    [[nodiscard]] std::size_t BootstrapCount() const override;
    [[nodiscard]] std::size_t BootstrapCount(Network::Protocol protocol) const override;
    // } IBootstrapCache
        
private:
    enum class CacheUpdate : std::uint32_t { Insert, Remove };
    struct CacheUpdates { 
        mutable std::mutex mutex;
        std::vector<std::pair<Network::RemoteAddress, CacheUpdate>> updates;
    };

    [[nodiscard]] Configuration::StatusCode Deserialize();
    [[nodiscard]] Configuration::StatusCode InitializeCache();
    [[nodiscard]] bool MaybeAddDefaultBootstrap(Network::Protocol protocol, BootstrapCache& bootstraps);

    std::shared_ptr<spdlog::logger> m_logger;
    std::shared_ptr<Scheduler::Delegate> m_spDelegate;
    IPeerMediator* m_mediator;

    std::filesystem::path m_filepath;
    BootstrapCache m_cache;
    CacheUpdates m_stage;
    DefaultBootstraps m_defaults;
};

//----------------------------------------------------------------------------------------------------------------------