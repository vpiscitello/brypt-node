//----------------------------------------------------------------------------------------------------------------------
// File: BootstrapService.cpp 
// Description:
//----------------------------------------------------------------------------------------------------------------------
#include "BootstrapService.hpp"
#include "Components/Scheduler/Delegate.hpp"
#include "Components/Scheduler/Service.hpp"
#include "Utilities/Assertions.hpp"
#include "Utilities/FileUtils.hpp"
#include "Utilities/Logger.hpp"
//----------------------------------------------------------------------------------------------------------------------
#include <lithium_json.hh>
//----------------------------------------------------------------------------------------------------------------------
#include <algorithm>
#include <cstdlib>
#include <iostream>
#include <fstream>
#include <ranges>
//----------------------------------------------------------------------------------------------------------------------

//----------------------------------------------------------------------------------------------------------------------
namespace {
namespace local {
//----------------------------------------------------------------------------------------------------------------------

struct BootstrapEntry;
struct EndpointEntry;

using BootstrapVector = std::vector<BootstrapEntry>;
using EndpointEntriesVector = std::vector<EndpointEntry>;

void ParseDefaultBootstraps(
    Configuration::Options::Endpoints const& endpoints, BootstrapService::DefaultBootstraps& defaults);
[[nodiscard]] bool HasDefaultBootstraps(BootstrapService::DefaultBootstraps& defaults);

//----------------------------------------------------------------------------------------------------------------------
} // local namespace
//----------------------------------------------------------------------------------------------------------------------
namespace defaults {
//----------------------------------------------------------------------------------------------------------------------

constexpr std::size_t FileSizeLimit = 8 * 1024 * 1024; // Limit bootstrap files to 8 MiB

//----------------------------------------------------------------------------------------------------------------------
} // default namespace
} // namespace
//----------------------------------------------------------------------------------------------------------------------

//----------------------------------------------------------------------------------------------------------------------
// Description: Symbol loading for JSON encoding. 
//----------------------------------------------------------------------------------------------------------------------
// [
//  {
//     "protocol": String,
//     "bootstraps": [
//         "target": String,
//     ...
//     ],
//  }
// ...,
// ]

#ifndef LI_SYMBOL_bootstraps
#define LI_SYMBOL_bootstraps
LI_SYMBOL(bootstraps)
#endif
#ifndef LI_SYMBOL_target
#define LI_SYMBOL_target
LI_SYMBOL(target)
#endif
#ifndef LI_SYMBOL_protocol
#define LI_SYMBOL_protocol
LI_SYMBOL(protocol)
#endif

//----------------------------------------------------------------------------------------------------------------------

struct local::BootstrapEntry
{
    BootstrapEntry() : target() {}
    explicit BootstrapEntry(std::string_view target) : target(target) { }
    std::string target;
};

//----------------------------------------------------------------------------------------------------------------------

struct local::EndpointEntry
{
    EndpointEntry() : protocol() , bootstraps() { }
    EndpointEntry(std::string_view protocol, BootstrapVector const& bootstraps)
        : protocol(protocol)
        , bootstraps(bootstraps)
    {}

    std::string protocol;
    BootstrapVector bootstraps;
};

//----------------------------------------------------------------------------------------------------------------------

BootstrapService::BootstrapService(
    std::filesystem::path const& filepath,
    Configuration::Options::Endpoints const& endpoints,
    bool useFilepathDeduction)
    : m_logger(spdlog::get(Logger::Name::Core.data()))
    , m_spDelegate()
    , m_mediator(nullptr)
    , m_filepath(filepath)
    , m_cache()
    , m_stage()
    , m_defaults()
{
    assert(m_logger);

    local::ParseDefaultBootstraps(endpoints, m_defaults);

    // An empty filepath indicates that filesystem usage has been disabled. The service can still be used to 
    // store runtime state, but no bootstraps will be cached between runs. 
    if (!m_filepath.empty()) {
        if (useFilepathDeduction) {
            // If required, fill in the default directory and filename to obtain a valid filepath.
            if (!m_filepath.has_filename()) { m_filepath = m_filepath / Configuration::DefaultBootstrapFilename; }
            if (!m_filepath.has_parent_path()) { m_filepath = Configuration::GetDefaultBryptFolder() / m_filepath; }
        }

        if (!FileUtils::CreateFolderIfNoneExist(m_filepath)) {
            m_logger->error("Failed to create the filepath at: {}!", m_filepath.string());
        }
    }
}

//----------------------------------------------------------------------------------------------------------------------

BootstrapService::~BootstrapService()
{
    [[maybe_unused]] auto const status = Serialize();
    assert(status == Configuration::StatusCode::Success); 
    
    m_spDelegate->Delist();
}

//----------------------------------------------------------------------------------------------------------------------

void BootstrapService::Register(IPeerMediator* const mediator)
{
    assert(!m_mediator);
    if (m_mediator = mediator; m_mediator) [[unlikely]] { m_mediator->RegisterObserver(this); }
}

//----------------------------------------------------------------------------------------------------------------------

void BootstrapService::Register(std::shared_ptr<Scheduler::Service> const& spScheduler)
{
    assert(Assertions::Threading::IsCoreThread());
    assert(!m_spDelegate);

    m_spDelegate = spScheduler->Register<BootstrapService>([this] () -> std::size_t {
        // Update the bootstrap cache with any changes that have occured since this last cycle. 
        auto const& [applied, difference] = UpdateCache();
        return applied;  // Provide the number of tasks executed to the scheduler. 
    }); 

    assert(m_spDelegate);
}

//----------------------------------------------------------------------------------------------------------------------

bool BootstrapService::FetchBootstraps()
{
    if (m_filepath.empty()) { return true; } // There is nothing to do if filesystem usage has been disabled. 

    Configuration::StatusCode status = (std::filesystem::exists(m_filepath)) ? Deserialize() : InitializeFile();
    bool const success = (!m_cache.empty() || status == Configuration::StatusCode::Success);
    return success;
}

//----------------------------------------------------------------------------------------------------------------------

void BootstrapService::InsertBootstrap(Network::RemoteAddress const& bootstrap)
{
    assert(bootstrap.IsValid()); // We should never be provided an invalid bootstrap.
    assert(Assertions::Threading::IsCoreThread()); // Currently, only the main thread has access to the bootstraps. 
    m_cache.emplace(bootstrap);
}

//----------------------------------------------------------------------------------------------------------------------

void BootstrapService::RemoveBootstrap(Network::RemoteAddress const& bootstrap)
{
    assert(bootstrap.IsValid());  // We should never be provided an invalid bootstrap.
    assert(Assertions::Threading::IsCoreThread()); // Currently, only the main thread has access to the bootstraps. 
    m_cache.erase(bootstrap);
}

//----------------------------------------------------------------------------------------------------------------------

void BootstrapService::OnRemoteConnected(Network::Endpoint::Identifier, Network::RemoteAddress const& address)
{
    if (!address.IsBootstrapable()) { return; }
    std::scoped_lock lock(m_stage.mutex);
    m_stage.updates.emplace_back(address, CacheUpdate::Insert);
    m_spDelegate->OnTaskAvailable(); // Notify the scheduler that a task is available to be executed. 
}

//----------------------------------------------------------------------------------------------------------------------

void BootstrapService::OnRemoteDisconnected(Network::Endpoint::Identifier, Network::RemoteAddress const& address)
{
    if (!address.IsBootstrapable()) { return; }
    std::scoped_lock lock(m_stage.mutex);
    m_stage.updates.emplace_back(address, CacheUpdate::Remove);
    m_spDelegate->OnTaskAvailable(); // Notify the scheduler that a task is available to be executed. 
}

//----------------------------------------------------------------------------------------------------------------------

BootstrapService::CacheUpdateResult BootstrapService::UpdateCache()
{
    // Note: A known limitation of the cache is that it does not handle updates for the same address from multiple
    // endpoints. If support for multiple connections to the same address is desired, the service will need to handle 
    // the associated edge cases. (e.g. { endpoint: 1, insert } > { endpoint: 2, insert } > { endpoint: 1, remove } ). 
    std::scoped_lock lock(m_stage.mutex);
    std::size_t const countBeforeUpdate = m_cache.size();
    std::ranges::for_each(m_stage.updates, [this] (auto& entry) {
        switch (entry.second) {
            case CacheUpdate::Insert: m_cache.emplace(std::move(entry.first)); break;
            case CacheUpdate::Remove: m_cache.erase(entry.first); break;
        }
    });
    std::size_t const applied = m_stage.updates.size();
    m_stage.updates.clear(); // Clear the stage for the next cycle.
    std::int32_t difference = static_cast<std::int32_t>(m_cache.size() - countBeforeUpdate);
    return { applied, difference };
}

//----------------------------------------------------------------------------------------------------------------------

Configuration::StatusCode BootstrapService::Serialize()
{
    using enum Configuration::StatusCode;
    assert(Assertions::Threading::IsCoreThread()); // Currently, only the main thread may serialize the bootstraps. 
    if (m_filepath.empty()) { return Success; } // If the filepath is empty, filesystem usage has been disabled. 

    UpdateCache(); // Collate any pending updates before the write. 

    std::ofstream writer(m_filepath, std::ofstream::out);
    if (writer.fail()) [[unlikely]] { m_logger->error("Failed to serialize bootstraps!"); return FileError; }

    // Transform the cache into sets mapped to the associated protocol. 
    using MappedCache = std::unordered_map<Network::Protocol, std::vector<BootstrapCache::const_iterator>>;
    MappedCache mapped;

    // Initialize the mapped cache with the configured protocols. 
    std::ranges::for_each(m_defaults, [this, &mapped] (auto const& entry) { 
        if (entry.second) { m_cache.emplace(*entry.second); }  // Ensure the cache always has the default bootstrap. 
        mapped.emplace(entry.first, std::vector<BootstrapCache::const_iterator>{}); 
    });

    // Map the bootstraps into protocol buckets.
    for (auto itr = m_cache.cbegin(); itr != m_cache.cend(); ++itr) {
        assert(itr->GetProtocol() != Network::Protocol::Invalid);
        mapped[itr->GetProtocol()].emplace_back(itr);
    }

    // Format and write out the cache. 
    writer << "[\n";
    std::ranges::for_each(mapped, [&writer, &mapped, written = std::size_t(0)] (auto const& group) mutable {
        writer << "\t{\n";
        writer << "\t\t\"protocol\": \"" << Network::ProtocolToString(group.first) << "\",\n";
        writer << "\t\t\"bootstraps\": [\n";
        std::ranges::for_each(group.second, [&writer, &group, written = std::size_t(0)] (auto const& bootstrap) mutable {
            writer << "\t\t\t{ \"target\": \"" << bootstrap->GetUri() << "\" }";
            if (++written != group.second.size()) { writer << ",\n"; }
        });
        writer << "\n\t\t]\n";
        writer << "\t}";
        if (++written != mapped.size()) { writer << ",\n"; }
    });
    writer << "\n]\n";
    writer.close();

    // If the failbit was set after the write, log an error and provide the caller with an error code. 
    if (writer.fail()) [[unlikely]] { m_logger->error("Failed to serialize bootstraps!"); return FileError; }

    return Success;
}

//----------------------------------------------------------------------------------------------------------------------

void BootstrapService::SetFilepath(std::filesystem::path const& filepath)
{
    assert(Assertions::Threading::IsCoreThread()); // Only the core thread should control the filepath. 
    m_filepath = filepath;
}

//----------------------------------------------------------------------------------------------------------------------

void BootstrapService::DisableFilesystem()
{
    assert(Assertions::Threading::IsCoreThread()); // Only the core thread should control the filepath. 
    m_filepath.clear();
}

//----------------------------------------------------------------------------------------------------------------------

bool BootstrapService::Contains(Network::RemoteAddress const& address) const
{
    return m_cache.contains(address);   
}

//----------------------------------------------------------------------------------------------------------------------

std::size_t BootstrapService::ForEachBootstrap(BootstrapReader const& reader) const
{
    using enum CallbackIteration;
    assert(Assertions::Threading::IsCoreThread()); // Currently, only the main thread has access to the bootstraps. 
    // Note: We only read the set of cached bootstraps. The core event loop should ensure the merge is run before 
    // processing messages that want the latest view of the data. 
    std::size_t read = 0;
    for (auto const& bootstrap : m_cache) {
        if (++read; reader(bootstrap) != Continue) { break; } // Break early if the reader indicates we should stop. 
    }
    return read;
}

//----------------------------------------------------------------------------------------------------------------------

std::size_t BootstrapService::ForEachBootstrap(Network::Protocol protocol, BootstrapReader const& reader) const
{
    using enum CallbackIteration;
    assert(Assertions::Threading::IsCoreThread()); // Currently, only the main thread has access to the bootstraps. 
    // Note: We only read the set of cached bootstraps. The core event loop should ensure the merge is run before 
    // processing messages that want the latest view of the data. 
    std::size_t read = 0;
    for (auto const& bootstrap : m_cache) {
        if (bootstrap.GetProtocol() != protocol) { continue; } // Skip any bootstrap not of the provided protocol.
        if (++read; reader(bootstrap) != Continue) { break; } // Break early if the reader indicates we should stop. 
    }
    return read;
}

//----------------------------------------------------------------------------------------------------------------------

std::size_t BootstrapService::BootstrapCount() const
{
    assert(Assertions::Threading::IsCoreThread()); // Currently, only the main thread has access to the bootstraps. 
    return m_cache.size();
}

//----------------------------------------------------------------------------------------------------------------------

std::size_t BootstrapService::BootstrapCount(Network::Protocol protocol) const
{
    assert(Assertions::Threading::IsCoreThread()); // Currently, only the main thread has access to the bootstraps. 
    return std::ranges::count_if(m_cache, [&protocol] (auto const& bootstrap) {
        return bootstrap.GetProtocol() == protocol;
    });
}

//----------------------------------------------------------------------------------------------------------------------

Configuration::StatusCode BootstrapService::Deserialize()
{
    using enum Configuration::StatusCode;
    assert(Assertions::Threading::IsCoreThread());
    if (m_filepath.empty()) { return Success; } // If the filepath is empty, filesystem usage has been disabled. 

    m_logger->debug("Reading bootstrap file at: {}.", m_filepath.string());

    // Determine the size of the file about to be read. Do not read a file that is above the given treshold. 
    {
        std::error_code error;
        auto const size = std::filesystem::file_size(m_filepath, error);
        if (error || size == 0 || size > defaults::FileSizeLimit) [[unlikely]] {
            return FileError;
        }
    }

    // Decode the JSON string into the bootstrap struct
    local::EndpointEntriesVector deserialized;
    {
        std::stringstream buffer;
        std::ifstream reader(m_filepath);
        if (reader.fail()) [[unlikely]] { return FileError; }
        buffer << reader.rdbuf(); // Read the file into the buffer stream

        std::string_view json = buffer.view();
        if (json.empty()) { return InputError; } // Report an error if there is nothing in the file. 

        auto const error = li::json_vector(s::protocol, s::bootstraps = li::json_vector(s::target))
            .decode(json, deserialized);
        
        // Report an error if the decoder reported an error or the desrialized container is still empty. 
        if (error.code || deserialized.empty()) { return DecodeError; }
    }

    // Transform the decoded bootstraps and emplace them into the cache.
    BootstrapCache bootstraps;
    bool hasTransformError = false; // A flag to detect decode errors when transforming the decoded bootstraps. 
    bootstraps.reserve(deserialized.size());
    std::ranges::for_each(deserialized, 
        [this, &bootstraps, &hasTransformError] (local::EndpointEntry& entry)
        {
            if (hasTransformError) { return; } // If we encoutnered a prior error continue from this iteration early.

            auto const protocol = Network::ParseProtocol(entry.protocol);
            if (protocol == Network::Protocol::Invalid) { hasTransformError = true; return; }
            
            bootstraps.reserve(bootstraps.size() + entry.bootstraps.size());
            std::ranges::for_each(entry.bootstraps, 
                [&] (Network::RemoteAddress bootstrap) {
                    // If there was a prior error or the bootstrap is invalid continue from this iteration early.
                    if (hasTransformError || !bootstrap.IsValid()) { hasTransformError = true; return; }
                    assert(bootstrap.IsBootstrapable()); // The address should always be marked as bootstrapable. 
                    bootstraps.emplace(std::move(bootstrap));
                }, 
                [&] (local::BootstrapEntry const& bootstrap) -> Network::RemoteAddress {
                    using Origin = Network::RemoteAddress::Origin;
                    return { protocol, bootstrap.target, true, Origin::Cache };
                });

            if (entry.bootstraps.empty() && !MaybeAddDefaultBootstrap(protocol, bootstraps)) {
                m_logger->warn("The {} network protocol has no associated bootstraps.", entry.protocol);
            }
        });

    m_cache = std::move(bootstraps);

    // If the cache is still empty when we have defauts an error occured. It's valid to have an empty cache when 
    // it is intentional to have this node not have initial connections (e.g. the first run of the "root" node).
    bool const error = hasTransformError || (m_cache.empty() && local::HasDefaultBootstraps(m_defaults));
    if (error) [[unlikely]] {
        m_logger->error("Failed to decode bootstrap file at: {}!", m_filepath.c_str());
        return DecodeError;
    }

    return Success;
}

//----------------------------------------------------------------------------------------------------------------------

Configuration::StatusCode BootstrapService::InitializeFile()
{
    using enum Configuration::StatusCode;
    assert(Assertions::Threading::IsCoreThread());
    if (m_filepath.empty()) { return Success; } // An empty filepath indicates serialization has been disabled. 

    m_logger->debug("Generating bootstrap file at: {}.", m_filepath.string());

    for (auto const& [protocol, optDefault] : m_defaults) {
        if (optDefault && optDefault->IsValid()) [[likely]] { m_cache.emplace(*optDefault); }
    }

    return Serialize();
}

//----------------------------------------------------------------------------------------------------------------------

bool BootstrapService::MaybeAddDefaultBootstrap(Network::Protocol protocol, BootstrapCache& bootstraps)
{
    if (auto const& optDefault = m_defaults[protocol]; optDefault && optDefault->IsValid()) {
        bootstraps.emplace(*optDefault);
        return true;
    }
    return false;
}

//----------------------------------------------------------------------------------------------------------------------

void local::ParseDefaultBootstraps(
    Configuration::Options::Endpoints const& endpoints, BootstrapService::DefaultBootstraps& defaults)
{
    for (auto const& options: endpoints) {
        if (auto const optBootstrap = options.GetBootstrap(); optBootstrap) {
            defaults.emplace(options.GetProtocol(), optBootstrap);
        } else {
            defaults.emplace(options.GetProtocol(), std::nullopt);
        }
    }
}

//----------------------------------------------------------------------------------------------------------------------

bool local::HasDefaultBootstraps(BootstrapService::DefaultBootstraps& defaults)
{
    return std::ranges::any_of(defaults | std::views::values,
        [] (auto const& optBootstrap) -> bool { return optBootstrap.has_value(); });
}

//----------------------------------------------------------------------------------------------------------------------
