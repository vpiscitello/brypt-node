//----------------------------------------------------------------------------------------------------------------------
// File: BootstrapService.cpp 
// Description:
//----------------------------------------------------------------------------------------------------------------------
#include "BootstrapService.hpp"
#include "Components/Scheduler/Delegate.hpp"
#include "Components/Scheduler/Registrar.hpp"
#include "Utilities/Assertions.hpp"
#include "Utilities/FileUtils.hpp"
#include "Utilities/Logger.hpp"
//----------------------------------------------------------------------------------------------------------------------
#include <boost/json.hpp>
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
//----------------------------------------------------------------------------------------------------------------------
namespace symbols {
//----------------------------------------------------------------------------------------------------------------------

constexpr std::string_view Bootstraps = "bootstraps";
constexpr std::string_view Target = "target";
constexpr std::string_view Protocol = "protocol";

//----------------------------------------------------------------------------------------------------------------------
} // symbols namespace
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

BootstrapService::BootstrapService()
    : m_logger(spdlog::get(Logger::Name.data()))
    , m_spDelegate()
    , m_pResolutionService(nullptr)
    , m_filepath()
    , m_cache()
    , m_stage()
    , m_defaults()
{
    assert(m_logger);
}

//----------------------------------------------------------------------------------------------------------------------

BootstrapService::BootstrapService(std::filesystem::path const& filepath, bool useFilepathDeduction)
    : m_logger(spdlog::get(Logger::Name.data()))
    , m_spDelegate()
    , m_pResolutionService(nullptr)
    , m_filepath(filepath)
    , m_cache()
    , m_stage()
    , m_defaults()
{
    assert(m_logger);

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
    // Note: There is a static destruction order issue caused by asserting the serialize method is only called on the 
    // core thread. This issue should only occur when the node core is declared as a static variable (e.g. in the 
    // shared library test suites). The assertion is still valuable, so the tests must ensure the static core variable
    // can be manually destroyed before implicit static destruction takes effect. 
    [[maybe_unused]] auto const status = Serialize();
    assert(status == Configuration::StatusCode::Success); 
}

//----------------------------------------------------------------------------------------------------------------------

std::filesystem::path const& BootstrapService::GetFilepath() const
{
    assert(Assertions::Threading::IsCoreThread()); // Only the core thread should control the filepath. 
    return m_filepath;
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

bool BootstrapService::FilesystemDisabled() const { return m_filepath.empty(); }

//----------------------------------------------------------------------------------------------------------------------

void BootstrapService::SetDefaults(Configuration::Options::Endpoints const& endpoints)
{
    local::ParseDefaultBootstraps(endpoints, m_defaults);
}

//----------------------------------------------------------------------------------------------------------------------

void BootstrapService::Register(IResolutionService* const pResolutionService)
{
    assert(!m_pResolutionService);
    if (m_pResolutionService = pResolutionService; m_pResolutionService) [[unlikely]] { 
        m_pResolutionService->RegisterObserver(this);
    }
}

//----------------------------------------------------------------------------------------------------------------------

void BootstrapService::Register(std::shared_ptr<Scheduler::Registrar> const& spRegistrar)
{
    assert(Assertions::Threading::IsCoreThread());
    assert(!m_spDelegate);

    m_spDelegate = spRegistrar->Register<BootstrapService>([this] (Scheduler::Frame const&) -> std::size_t {
        // Update the bootstrap cache with any changes that have occurred since this last cycle. 
        auto const& [applied, difference] = UpdateCache();
        return applied;  // Provide the number of tasks executed to the scheduler. 
    }); 

    assert(m_spDelegate);
}

//----------------------------------------------------------------------------------------------------------------------

void BootstrapService::UnregisterServices()
{
    assert(Assertions::Threading::IsCoreThread());
    m_pResolutionService = nullptr;
    m_spDelegate->Delist();
    m_spDelegate.reset();
}

//----------------------------------------------------------------------------------------------------------------------

bool BootstrapService::FetchBootstraps()
{
    // We should not process the file if the cache is in use. 
    if (!m_cache.empty()) { return true; }

    // We should not process the file filesystem usage has been disabled, but we should initialize the cache
    // with the user provided defaults. 
    if (m_filepath.empty()) { return InitializeCache() == Configuration::StatusCode::Success; }

    Configuration::StatusCode status = (std::filesystem::exists(m_filepath)) ? Deserialize() : InitializeCache();
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

    BootstrapCache bootstraps;
    bool hasTransformError = false; // A flag to detect decode errors when transforming the decoded bootstraps. 

    try {
        constexpr boost::json::parse_options ParserOptions{
            .allow_comments = true,
            .allow_trailing_commas = true,
        };

        std::stringstream buffer;
        {
            std::ifstream reader{ m_filepath };
            if (reader.fail()) [[unlikely]] {
                return FileError;
            }
            buffer << reader.rdbuf(); // Read the file into the buffer stream.
        }

        auto const serialized = buffer.view();
        if (serialized.empty()) {
            return InputError;
        }

        boost::json::error_code error;
        auto const json = boost::json::parse(buffer.view(), error, boost::json::storage_ptr{}, ParserOptions).as_array();
        if (error) {
            constexpr std::string_view DefaultContext = "[\n\n]\n";
            if (serialized == DefaultContext) { return Success; } // We shouldn't consider an empty file an error.
            DisableFilesystem(); // Prevent the malformed file from being overwritten. 
            return DecodeError;
        }

        // Transform the decoded bootstraps and emplace them into the cache.
        bootstraps.reserve(json.size());

        for (auto const& group : json) {
            boost::json::object const& object = group.as_object();

            auto const protocolField = object.at(symbols::Protocol).as_string();
            auto const protocol = Network::ParseProtocol(protocolField.c_str());
            if (protocol == Network::Protocol::Invalid) {
                hasTransformError = true;
                break;
            }

            std::ranges::for_each(object.at(symbols::Bootstraps).as_array(),
                [&] (Network::RemoteAddress bootstrap) {
                    // If there was a prior error or the bootstrap is invalid continue from this iteration early.
                    if (hasTransformError || !bootstrap.IsValid()) { 
                        hasTransformError = true;
                        return;
                    }
                    assert(bootstrap.IsBootstrapable()); // The address should always be marked as bootstrapable. 
                    bootstraps.emplace(std::move(bootstrap));
                },
                [&] (boost::json::value const& bootstrap) -> Network::RemoteAddress {
                    using Origin = Network::RemoteAddress::Origin;
                    return { protocol, bootstrap.at(symbols::Target).as_string(), true, Origin::Cache };
                });

            if (!MaybeAddDefaultBootstrap(protocol, bootstraps)) {
                m_logger->warn("The {} network protocol has no associated bootstraps.", protocolField.c_str());
            }
        }
    } catch (...) {
        hasTransformError = true;
    }

    m_cache = std::move(bootstraps);

    // If the cache is still empty when we have defauts an error occurred. It's valid to have an empty cache when 
    // it is intentional to have this node not have initial connections (e.g. the first run of the "root" node).
    bool const error = hasTransformError || (m_cache.empty() && local::HasDefaultBootstraps(m_defaults));
    if (error) [[unlikely]] {
        #if defined(WIN32)
        m_logger->error(L"Failed to decode bootstrap file at: {}!", m_filepath.c_str());
        #else
        m_logger->error("Failed to decode bootstrap file at: {}!", m_filepath.c_str());
        #endif

        DisableFilesystem(); // Prevent writing out to the malformed file in the event we can't parse it. 
        
        return DecodeError;
    }

    return Success;
}

//----------------------------------------------------------------------------------------------------------------------

Configuration::StatusCode BootstrapService::InitializeCache()
{
    using enum Configuration::StatusCode;
    assert(Assertions::Threading::IsCoreThread());

    for (auto const& [protocol, optDefault] : m_defaults) {
        if (optDefault && optDefault->IsValid()) [[likely]] { m_cache.emplace(*optDefault); }
    }

    if (m_filepath.empty()) { return Success; } // If filesystem usage is disabled, there is nothing more to do. 

    m_logger->debug("Generating bootstrap file at: {}.", m_filepath.string());
    return Serialize();
}

//----------------------------------------------------------------------------------------------------------------------

bool BootstrapService::MaybeAddDefaultBootstrap(Network::Protocol protocol, BootstrapCache& bootstraps)
{
    if (auto const& optDefault = m_defaults[protocol]; optDefault && optDefault->IsValid()) {
        // Configured bootstraps are considered to have an origin from the user (such that they can receive connection 
        // events). If the default bootstrap exists in the cache, it needs to be removed before re-adding it to correct
        // the origin. 
        if (auto itr = m_cache.find(*optDefault); itr != m_cache.end()) { m_cache.erase(itr); }
        assert(optDefault->GetOrigin() == Network::RemoteAddress::Origin::User);
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
