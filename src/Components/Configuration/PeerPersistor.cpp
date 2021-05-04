//----------------------------------------------------------------------------------------------------------------------
// File: ConfigurationManager.cpp 
// Description:
//----------------------------------------------------------------------------------------------------------------------
#include "PeerPersistor.hpp"
#include "BryptIdentifier/BryptIdentifier.hpp"
#include "BryptIdentifier/ReservedIdentifiers.hpp"
#include "Utilities/FileUtils.hpp"
#include "Utilities/LogUtils.hpp"
//----------------------------------------------------------------------------------------------------------------------
#include <lithium_json.hh>
//----------------------------------------------------------------------------------------------------------------------
#include <cstdlib>
#include <iostream>
#include <fstream>
#include <optional>
#include <vector>
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
    Configuration::EndpointsSet const& endpoints,  PeerPersistor::DefaultBootstrapMap& defaults);
void FillDefaultBootstrap(
    PeerPersistor::UniqueBootstrapSet const& upBootstraps, std::optional<Network::RemoteAddress> const& optDefault);

void WriteEndpointPeers(PeerPersistor::UniqueProtocolMap const& upProtocols, std::ofstream& out);

//----------------------------------------------------------------------------------------------------------------------
} // local namespace
//----------------------------------------------------------------------------------------------------------------------
namespace defaults {
//----------------------------------------------------------------------------------------------------------------------

constexpr std::uint32_t FileSizeLimit = 12'000; // Limit the peers files to 12KB

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
    BootstrapEntry()
        : target()
    {
    }
    explicit BootstrapEntry(std::string_view target)
        : target(target)
    {
    }

    std::string target;
};

//----------------------------------------------------------------------------------------------------------------------

struct local::EndpointEntry
{
    EndpointEntry()
        : protocol()
        , bootstraps()
    {
    }

    EndpointEntry(
        std::string_view protocol,
        BootstrapVector const& bootstraps)
        : protocol(protocol)
        , bootstraps(bootstraps)
    {
    }

    std::string protocol;
    BootstrapVector bootstraps;
};

//----------------------------------------------------------------------------------------------------------------------

PeerPersistor::PeerPersistor(
    std::filesystem::path const& filepath, Configuration::EndpointsSet const& endpoints, bool shouldBuildPath)
    : m_spLogger(spdlog::get(LogUtils::Name::Core.data()))
    , m_mediator(nullptr)
    , m_fileMutex()
    , m_filepath(filepath)
    , m_protocolsMutex()
    , m_upProtocols()
    , m_defaults()
{
    assert(m_spLogger);

    local::ParseDefaultBootstraps(endpoints, m_defaults);

    if (!shouldBuildPath) { return; }

    // If the filepath does not have a filename, attach the default config.json
    if (!m_filepath.has_filename()) {
        m_filepath = m_filepath / Configuration::DefaultKnownPeersFilename;
    }

    // If the filepath does not have a parent path, get and attach the default brypt folder
    if (!m_filepath.has_parent_path()) {
        m_filepath = Configuration::GetDefaultBryptFolder() / m_filepath;
    }

    if (!FileUtils::CreateFolderIfNoneExist(m_filepath)) {
        m_spLogger->error("Failed to create the filepath at: {}!", m_filepath.string());
        return;
    }
}

//----------------------------------------------------------------------------------------------------------------------

void PeerPersistor::SetMediator(IPeerMediator* const mediator)
{
    // If there already a mediator attached to the persistor, unpublish the persistor
    // from the previous mediator.
    if (m_mediator) [[likely]] { m_mediator->UnpublishObserver(this); }
    
    // Set the mediator, it is possible for the nullptr if the caller intends to 
    // detach the previous mediator.
    m_mediator = mediator;

    // If the mediator exists, register the persistor with the mediator
    if (m_mediator) [[likely]] { m_mediator->RegisterObserver(this); }
}

//----------------------------------------------------------------------------------------------------------------------

bool PeerPersistor::FetchBootstraps()
{
    Configuration::StatusCode status;
    if (std::filesystem::exists(m_filepath)) {
        m_spLogger->debug("Reading peers file at: {}.", m_filepath.string());
        status = DecodePeersFile();
    } else {
        m_spLogger->debug("Generating peers file at: {}.", m_filepath.string());
        status = SetupPeersFile();
    }

    if (!m_upProtocols || status != Configuration::StatusCode::Success) {
        m_spLogger->error("Failed to decode peers file at: {}!", m_filepath.string());
        return false;
    }

    return true;
}

//----------------------------------------------------------------------------------------------------------------------

Configuration::StatusCode PeerPersistor::Serialize()
{
    std::scoped_lock lock(m_fileMutex);
    
    if (!m_upProtocols) [[unlikely]] { return Configuration::StatusCode::InputError; }
    if (m_filepath.empty()) [[unlikely]] { return Configuration::StatusCode::FileError; }

    std::ofstream out(m_filepath, std::ofstream::out);
    if (out.fail()) [[unlikely]] { return Configuration::StatusCode::FileError; }

    local::WriteEndpointPeers(m_upProtocols, out);

    out.close();

    return Configuration::StatusCode::Success;
}

//----------------------------------------------------------------------------------------------------------------------

Configuration::StatusCode PeerPersistor::DecodePeersFile()
{
    // Determine the size of the file about to be read. Do not read a file
    // that is above the given treshold. 
    std::error_code error;
    auto const size = std::filesystem::file_size(m_filepath, error);
    if (error || size == 0 || size > defaults::FileSizeLimit) [[unlikely]] {
        return Configuration::StatusCode::FileError;
    }

    // Attempt to open the configuration file, if it fails return noopt
    std::ifstream file(m_filepath);
    if (file.fail()) [[unlikely]] { return Configuration::StatusCode::FileError; }

    std::stringstream buffer;
    buffer << file.rdbuf(); // Read the file into the buffer stream
    std::string json = buffer.str(); // Put the file contents into a string to be trimmed and parsed
    // Remove newlines and tabs from the string
    json.erase(std::remove_if(json.begin(), json.end(), &FileUtils::IsNewlineOrTab), json.end());

    local::EndpointEntriesVector endpoints;

    // Decode the JSON string into the configuration struct
    li::json_vector(
        s::protocol,
        s::bootstraps = li::json_vector(s::target)).decode(json, endpoints);

    auto upProtocols = std::make_unique<ProtocolMap>();
    upProtocols->reserve(endpoints.size());

    std::for_each(
        endpoints.begin(), endpoints.end(),
        [&] (local::EndpointEntry& endpoint)
        {
            // Parse the protocol name from the entry, if it is not a valid name continue to 
            // the next endpoint entry.
            auto const protocol = Network::ParseProtocol(endpoint.protocol);
            if (protocol == Network::Protocol::Invalid) { return; }

            auto upBootstraps = std::make_unique<BootstrapSet>();

            auto& bootstraps = endpoint.bootstraps;
            upBootstraps->reserve(bootstraps.size());

            if (!bootstraps.empty()) {
                std::for_each(
                    bootstraps.begin(), bootstraps.end(),
                    [&protocol, &upBootstraps] (local::BootstrapEntry const& bootstrap)
                    {
                        if (bootstrap.target.empty()) { return; }
                        Network::RemoteAddress address(protocol, bootstrap.target, true);
                        upBootstraps->emplace(std::move(address));
                    });
            } else {
                // Insert any valid peers into the endpoint map
                if (auto const itr = m_defaults.find(protocol); itr != m_defaults.end()) {
                    auto const& [key, optDefaultBootstrap] = *itr;
                    local::FillDefaultBootstrap(upBootstraps, optDefaultBootstrap);
                }
            }
            
            upProtocols->emplace(protocol, std::move(upBootstraps));
        }
    );

    if (upProtocols->empty()) [[unlikely]] { return Configuration::StatusCode::DecodeError; }

    m_upProtocols = std::move(upProtocols);
    return Configuration::StatusCode::Success;
}

//----------------------------------------------------------------------------------------------------------------------

//----------------------------------------------------------------------------------------------------------------------
// Description: Generates a new configuration file based on user handler line arguements or
// from user input.
//----------------------------------------------------------------------------------------------------------------------
Configuration::StatusCode PeerPersistor::SerializeEndpointPeers()
{
    if (!m_upProtocols) [[unlikely]] { return Configuration::StatusCode::DecodeError; }

    for (auto const& [endpoint, upBootstraps] : *m_upProtocols) {
        if (upBootstraps->empty()) [[unlikely]] {
            auto const protocol = Network::ProtocolToString(endpoint);
            m_spLogger->warn("{} has no attached bootstrap peers.", protocol);
        }
    }

    Configuration::StatusCode const status = Serialize();
    if (status != Configuration::StatusCode::Success) [[unlikely]] {
        m_spLogger->error("Failed to serialize peers!");
    }

    return status;
}

//----------------------------------------------------------------------------------------------------------------------

//----------------------------------------------------------------------------------------------------------------------
// Description: Initializes and saves a peer file.
//----------------------------------------------------------------------------------------------------------------------
Configuration::StatusCode PeerPersistor::SetupPeersFile()
{
    if (!m_upProtocols) [[unlikely]] { m_upProtocols = std::make_unique<ProtocolMap>(); }

    for (auto const& [protocol, optDefault] : m_defaults) {
        auto upBootstraps = std::make_unique<BootstrapSet>();
        if (optDefault && optDefault->IsValid()) [[likely]] {
            upBootstraps->emplace(*optDefault);
        }
        m_upProtocols->emplace(protocol, std::move(upBootstraps));
    }

    Configuration::StatusCode const status = Serialize();
    if (status != Configuration::StatusCode::Success) [[unlikely]] {
        m_spLogger->error("Failed to serialize peers!");
    }

    return status;
}

//----------------------------------------------------------------------------------------------------------------------

void PeerPersistor::AddBootstrapEntry(
    std::shared_ptr<Peer::Proxy> const& spPeerProxy, Network::Endpoint::Identifier identifier)
{
    // Get the entry from the peer, if there is no entry there is nothing to store. 
    if (auto const optBootstrap = spPeerProxy->GetRegisteredAddress(identifier); optBootstrap) {
        AddBootstrapEntry(*optBootstrap);
    }
}

//----------------------------------------------------------------------------------------------------------------------

void PeerPersistor::AddBootstrapEntry(Network::RemoteAddress const& bootstrap)
{
    if (!bootstrap.IsValid()) [[unlikely]] { return; }

    {
        std::scoped_lock lock(m_protocolsMutex);
        // Since we always want ensure the peer can be tracked, we use emplace to either
        // insert a new entry for the technolgoy type or get the existing entry.
        auto const [itr, emplaced] = m_upProtocols->try_emplace(bootstrap.GetProtocol()); 
        if (itr != m_upProtocols->end()) {
            auto const& [key, upBootstraps] = *itr;
            if (upBootstraps) [[likely]] { upBootstraps->emplace(bootstrap); }
        }

        // Write the updated peers to the file
        [[maybe_unused]] auto const status = Serialize();
    }
}

//----------------------------------------------------------------------------------------------------------------------

void PeerPersistor::DeleteBootstrapEntry(
    std::shared_ptr<Peer::Proxy> const& spPeerProxy, Network::Endpoint::Identifier identifier)
{
    // Get the entry from the peer, if there is no entry there is nothing to delete. 
    if (auto const optBootstrap = spPeerProxy->GetRegisteredAddress(identifier); optBootstrap) {
        DeleteBootstrapEntry(*optBootstrap);
    }
}

//----------------------------------------------------------------------------------------------------------------------

void PeerPersistor::DeleteBootstrapEntry(Network::RemoteAddress const& bootstrap)
{
    if (!bootstrap.IsValid()) [[unlikely]] { return; }

    {
        std::scoped_lock lock(m_protocolsMutex);
        // Since we always want ensure the peer can be tracked, we use emplace to either
        // insert a new entry for the technolgoy type or get the existing entry.
        auto const [itr, emplaced] = m_upProtocols->try_emplace(bootstrap.GetProtocol());
        if (itr != m_upProtocols->end()) {
            auto const& [key, upBootstraps] = *itr;
            upBootstraps->erase(bootstrap);
        }

        // Write the updated peers to the file
        [[maybe_unused]] auto const status = Serialize();
    }
}

//----------------------------------------------------------------------------------------------------------------------

void PeerPersistor::HandlePeerStateChange(
    std::weak_ptr<Peer::Proxy> const& wpPeerProxy,
    Network::Endpoint::Identifier identifier,
    [[maybe_unused]] Network::Protocol protocol,
    ConnectionState change)
{
    // If the persistor peers have not yet been intialized, simply return
    if (!m_upProtocols) [[unlikely]] { return; }

    if (auto const spPeerProxy = wpPeerProxy.lock(); spPeerProxy) {
        switch (change) {
            case ConnectionState::Connected: {
                AddBootstrapEntry(spPeerProxy, identifier);
            } break;
            case ConnectionState::Disconnected:{
                DeleteBootstrapEntry(spPeerProxy, identifier);
            } break;
            default: return; // Currently, we don't persist information for other state changes.
        }
    }
}

//----------------------------------------------------------------------------------------------------------------------

bool PeerPersistor::ForEachCachedBootstrap(
    AllProtocolsReadFunction const& callback, AllProtocolsErrorFunction const& error) const
{
    std::scoped_lock lock(m_protocolsMutex);
    if (!m_upProtocols) { return false; }

    for (auto const& [protocol, upBootstraps]: *m_upProtocols) {
        // Notify the caller that there are no listed peers for an endpoint of a given protocol
        if (!upBootstraps) { error(protocol); continue; }

        CallbackIteration result;
        for (auto const& bootstrap: *upBootstraps) {
            result = callback(bootstrap);
            if (result != CallbackIteration::Continue) { break; }
        }
        if (result != CallbackIteration::Continue) { break; }
    }

    return true;
}

//----------------------------------------------------------------------------------------------------------------------

bool PeerPersistor::ForEachCachedBootstrap(
    Network::Protocol protocol, OneProtocolReadFunction const& callback) const
{
    std::scoped_lock lock(m_protocolsMutex);
    if (!m_upProtocols) [[unlikely]] { return false; }

    auto const itr = m_upProtocols->find(protocol);
    if (itr == m_upProtocols->end()) { return false; }   

    auto const& [key, upBootstraps] = *itr;
    for (auto const& bootstrap: *upBootstraps) {
        auto const result = callback(bootstrap);
        if (result != CallbackIteration::Continue) { break; }
    }

    return true;
}

//----------------------------------------------------------------------------------------------------------------------

std::size_t PeerPersistor::CachedBootstrapCount() const
{
    std::scoped_lock lock(m_protocolsMutex);
    if (!m_upProtocols) [[unlikely]] { return 0; }

    std::size_t count = 0;
    for (auto const& [protocol, upBootstraps]: *m_upProtocols) {
        if (upBootstraps) { count += upBootstraps->size(); }
    }

    return count;
}

//----------------------------------------------------------------------------------------------------------------------

std::size_t PeerPersistor::CachedBootstrapCount(Network::Protocol protocol) const
{
    std::scoped_lock lock(m_protocolsMutex);
    if (!m_upProtocols) [[unlikely]] { return 0; }

    if (auto const itr = m_upProtocols->find(protocol); itr != m_upProtocols->end()) {
        auto const& [protocol, upBootstraps] = *itr;
        if (upBootstraps) { return upBootstraps->size(); }
    }   
    return 0;
}

//----------------------------------------------------------------------------------------------------------------------

void local::ParseDefaultBootstraps(
    Configuration::EndpointsSet const& endpoints, PeerPersistor::DefaultBootstrapMap& defaults)
{
    for (auto const& options: endpoints) {
        if (auto const optBootstrap = options.GetBootstrap(); optBootstrap) {
            defaults.emplace(options.type, optBootstrap);
        } else {
            defaults.emplace(options.type, std::nullopt);
        }
    }
}

//----------------------------------------------------------------------------------------------------------------------

void local::FillDefaultBootstrap(
    PeerPersistor::UniqueBootstrapSet const& upBootstraps,
    std::optional<Network::RemoteAddress> const& optDefault)
{
    assert(upBootstraps);
    if (optDefault) {
        if (!optDefault->IsValid()) [[unlikely]] { return; }
        upBootstraps->emplace(*optDefault);
    }
}

//----------------------------------------------------------------------------------------------------------------------

void local::WriteEndpointPeers(
    PeerPersistor::UniqueProtocolMap const& upProtocols, std::ofstream& out)
{
    // If the provided endpoint peers are empty, ther is nothing to do
    if (!upProtocols) [[unlikely]] { return; }

    std::size_t endpointsWritten = 0;
    std::size_t const endpointSize = upProtocols->size();
    out << "[\n";
    for (auto const& [protocol, upBootstraps]: *upProtocols) {
        if (protocol == Network::Protocol::Invalid){ continue; }
        
        out << "\t{\n";
        out << "\t\t\"protocol\": \"" << Network::ProtocolToString(protocol) << "\",\n";
        out << "\t\t\"bootstraps\": [\n";

        std::size_t peersWritten = 0;
        std::size_t const peersSize = upBootstraps->size();
        for (auto const& bootstrap: *upBootstraps) {
            out << "\t\t\t{ \"target\": \"" << bootstrap.GetUri() << "\" }";
            if (++peersWritten != peersSize) { out << ",\n"; }
        }

        out << "\n\t\t]\n";
        out << "\t}";
        if (++endpointsWritten != endpointSize) { out << ",\n"; }
    }
    out << "\n]\n";
}

//----------------------------------------------------------------------------------------------------------------------
