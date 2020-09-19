//------------------------------------------------------------------------------------------------
// File: ConfigurationManager.cpp 
// Description:
//-----------------------------------------------------------------------------------------------
#include "PeerPersistor.hpp"
#include "../BryptIdentifier/BryptIdentifier.hpp"
#include "../BryptIdentifier/ReservedIdentifiers.hpp"
#include "../Utilities/FileUtils.hpp"
//-----------------------------------------------------------------------------------------------
#include "../Libraries/metajson/metajson.hh"
#include <cstdlib>
#include <iostream>
#include <fstream>
#include <optional>
#include <vector>
//-----------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
namespace {
namespace local {
//------------------------------------------------------------------------------------------------

struct TBootstrapEntry;
struct TEndpointEntry;

using BootstrapVector = std::vector<TBootstrapEntry>;
using EndpointEntriesVector = std::vector<TEndpointEntry>;

void ParseDefaultBootstraps(
    Configuration::EndpointConfigurations const& configurations, 
    CPeerPersistor::DefaultBootstrapMap& defaults);
void FillDefaultBootstrap(BootstrapVector& bootstraps, std::string_view target);

void WriteEndpointPeers(
    CPeerPersistor::UniqueEndpointBootstrapMap const& endpoints,
    std::ofstream& out);

//------------------------------------------------------------------------------------------------
} // local namespace
//------------------------------------------------------------------------------------------------
namespace defaults {
//------------------------------------------------------------------------------------------------

constexpr std::uint32_t FileSizeLimit = 12'000; // Limit the peers files to 12KB

//------------------------------------------------------------------------------------------------
} // default namespace
} // namespace
//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
// Description: Symbol loading for JSON encoding. 
//------------------------------------------------------------------------------------------------
// [
//  {
//     "technology": String,
//     "bootstraps": [
//         "target": String,
//     ...
//     ],
//  }
// ...,
// ]

IOD_SYMBOL(bootstraps)
IOD_SYMBOL(target)
IOD_SYMBOL(technology)

//------------------------------------------------------------------------------------------------

struct local::TBootstrapEntry
{
    TBootstrapEntry()
        : target()
    {
    }
    TBootstrapEntry(std::string_view target)
        : target(target)
    {
    }

    std::string target;
};

//------------------------------------------------------------------------------------------------

struct local::TEndpointEntry
{
    TEndpointEntry()
        : technology()
        , bootstraps()
    {
    }

    TEndpointEntry(
        std::string_view technology,
        BootstrapVector const& bootstraps)
        : technology(technology)
        , bootstraps(bootstraps)
    {
    }

    std::string technology;
    BootstrapVector bootstraps;
};

//-----------------------------------------------------------------------------------------------

CPeerPersistor::CPeerPersistor()
    : m_mediator(nullptr)
    , m_fileMutex()
    , m_filepath()
    , m_endpointBootstrapsMutex()
    , m_upEndpointBootstraps()
    , m_defaults()
{
    m_filepath = Configuration::GetDefaultPeersFilepath();
    FileUtils::CreateFolderIfNoneExist(m_filepath);
}

//-----------------------------------------------------------------------------------------------

CPeerPersistor::CPeerPersistor(std::string_view filepath)
    : m_mediator(nullptr)
    , m_fileMutex()
    , m_filepath(filepath)
    , m_endpointBootstrapsMutex()
    , m_upEndpointBootstraps()
    , m_defaults()
{
    // If the filepath does not have a filename, attach the default config.json
    if (!m_filepath.has_filename()) {
        m_filepath = m_filepath / Configuration::DefaultKnownPeersFilename;
    }

    // If the filepath does not have a parent path, get and attach the default brypt folder
    if (!m_filepath.has_parent_path()) {
        m_filepath = Configuration::GetDefaultBryptFolder() / m_filepath;
    }

    FileUtils::CreateFolderIfNoneExist(m_filepath);
}

//-----------------------------------------------------------------------------------------------

CPeerPersistor::CPeerPersistor(Configuration::EndpointConfigurations const& configurations)
    : m_mediator(nullptr)
    , m_fileMutex()
    , m_filepath()
    , m_endpointBootstrapsMutex()
    , m_upEndpointBootstraps()
    , m_defaults()
{
    m_filepath = Configuration::GetDefaultPeersFilepath();
    FileUtils::CreateFolderIfNoneExist(m_filepath);

    local::ParseDefaultBootstraps(configurations, m_defaults);
}

//-----------------------------------------------------------------------------------------------

CPeerPersistor::CPeerPersistor(
    std::string_view filepath,
    Configuration::EndpointConfigurations const& configurations)
    : m_mediator(nullptr)
    , m_fileMutex()
    , m_filepath(filepath)
    , m_endpointBootstrapsMutex()
    , m_upEndpointBootstraps()
    , m_defaults()
{
    // If the filepath does not have a filename, attach the default config.json
    if (!m_filepath.has_filename()) {
        m_filepath = m_filepath / Configuration::DefaultKnownPeersFilename;
    }

    // If the filepath does not have a parent path, get and attach the default brypt folder
    if (!m_filepath.has_parent_path()) {
        m_filepath = Configuration::GetDefaultBryptFolder() / m_filepath;
    }

    FileUtils::CreateFolderIfNoneExist(m_filepath);

    local::ParseDefaultBootstraps(configurations, m_defaults);
}

//-----------------------------------------------------------------------------------------------

void CPeerPersistor::SetMediator(IPeerMediator* const mediator)
{
    // If there already a mediator attached to the persistor, unpublish the persistor
    // from the previous mediator.
    if (m_mediator) {
        mediator->UnpublishObserver(this);
    }
    
    // Set the mediator, it is possible for the nullptr if the caller intends to 
    // detach the previous mediator.
    m_mediator = mediator;

    // If the mediator exists, register the persistor with the mediator
    if (m_mediator) {
        m_mediator->RegisterObserver(this);
    }
}

//-----------------------------------------------------------------------------------------------

bool CPeerPersistor::FetchBootstraps()
{
    Configuration::StatusCode status = Configuration::StatusCode::DecodeError;
    if (std::filesystem::exists(m_filepath)) {
        NodeUtils::printo(
            "Reading peers file at: " + m_filepath.string(),
            NodeUtils::PrintType::Node);
        status = DecodePeersFile();
    } else {
        status = SetupPeersFile();
    }

    if (!m_upEndpointBootstraps || status != Configuration::StatusCode::Success) {
        NodeUtils::printo(
            "Failed to decode peers file at: " + m_filepath.string(),
            NodeUtils::PrintType::Node);
        return false;
    }

    return true;
}

//-----------------------------------------------------------------------------------------------

Configuration::StatusCode CPeerPersistor::Serialize()
{
    std::scoped_lock lock(m_fileMutex);
    
    if (!m_upEndpointBootstraps) {
        return Configuration::StatusCode::InputError;
    }

    if (m_filepath.empty()) {
        return Configuration::StatusCode::FileError;
    }

    std::ofstream out(m_filepath, std::ofstream::out);
    if (out.fail()) {
        return Configuration::StatusCode::FileError;
    }

    local::WriteEndpointPeers(m_upEndpointBootstraps, out);

    out.close();

    return Configuration::StatusCode::Success;
}

//-----------------------------------------------------------------------------------------------

Configuration::StatusCode CPeerPersistor::DecodePeersFile()
{
    // Determine the size of the file about to be read. Do not read a file
    // that is above the given treshold. 
    std::error_code error;
    auto const size = std::filesystem::file_size(m_filepath, error);
    if (error || size == 0 || size > defaults::FileSizeLimit) {
        return Configuration::StatusCode::FileError;
    }

    // Attempt to open the configuration file, if it fails return noopt
    std::ifstream file(m_filepath);
    if (file.fail()) {
        return Configuration::StatusCode::FileError;
    }

    std::stringstream buffer;
    buffer << file.rdbuf(); // Read the file into the buffer stream
    std::string json = buffer.str(); // Put the file contents into a string to be trimmed and parsed
    // Remove newlines and tabs from the string
    json.erase(std::remove_if(json.begin(), json.end(), &FileUtils::IsNewlineOrTab), json.end());

    local::EndpointEntriesVector endpoints;

    // Decode the JSON string into the configuration struct
    iod::json_vector(
        s::technology,
        s::bootstraps = iod::json_vector(s::target)).decode(json, endpoints);

    auto upEndpointBootstraps = std::make_unique<EndpointBootstrapMap>();
    upEndpointBootstraps->reserve(endpoints.size());

    std::for_each(
        endpoints.begin(), endpoints.end(),
        [&] (local::TEndpointEntry& endpoint)
        {
            // Parse the technology name from the entry, if it is not a valid name continue to 
            // the next endpoint entry.
            auto const technology = Endpoints::ParseTechnologyType(endpoint.technology);
            if (technology == Endpoints::TechnologyType::Invalid) {
                return;
            }

            auto upBootstraps = std::make_unique<BootstrapSet>();

            auto& bootstraps = endpoint.bootstraps;
            upBootstraps->reserve(bootstraps.size());

            if (bootstraps.empty()) {
                if (auto const itr = m_defaults.find(technology); itr != m_defaults.end()) {
                    auto const& [key, defaultBootstrap] = *itr;
                    local::FillDefaultBootstrap(bootstraps, defaultBootstrap);
                }
            }

            // Insert any valid peers into the endpoint map
            std::for_each(
                bootstraps.begin(), bootstraps.end(),
                [&technology, &upBootstraps] (local::TBootstrapEntry const& bootstrap)
                {
                    if (bootstrap.target.empty()) {
                        return;
                    }

                    upBootstraps->emplace(bootstrap.target);
                }
            );
            
            upEndpointBootstraps->emplace(technology, std::move(upBootstraps));
        }
    );

    if (upEndpointBootstraps->empty()) {
        return Configuration::StatusCode::DecodeError;
    }

    m_upEndpointBootstraps = std::move(upEndpointBootstraps);
    return Configuration::StatusCode::Success;
}

//-----------------------------------------------------------------------------------------------

//-----------------------------------------------------------------------------------------------
// Description: Generates a new configuration file based on user command line arguements or
// from user input.
//-----------------------------------------------------------------------------------------------
Configuration::StatusCode CPeerPersistor::SerializeEndpointPeers()
{
    if (!m_upEndpointBootstraps) {
        return Configuration::StatusCode::DecodeError;
    }

    for (auto const& [endpoint, upBootstraps] : *m_upEndpointBootstraps) {
        if (upBootstraps->empty()) {
            auto const technology = Endpoints::TechnologyTypeToString(endpoint);
            NodeUtils::printo(
                "Warning: " + technology + " has no attached bootstrap peers!",
                NodeUtils::PrintType::Node);
        }
    }

    Configuration::StatusCode const status = Serialize();
    if (status != Configuration::StatusCode::Success) {
        NodeUtils::printo("Warning: Filed to serialize peers!", NodeUtils::PrintType::Node);
    }

    return status;
}

//-----------------------------------------------------------------------------------------------

//-----------------------------------------------------------------------------------------------
// Description: Initializes and saves a peer file.
//-----------------------------------------------------------------------------------------------
Configuration::StatusCode CPeerPersistor::SetupPeersFile()
{
    if (!m_upEndpointBootstraps) {
        m_upEndpointBootstraps = std::make_unique<EndpointBootstrapMap>();
    }

    for (auto const& [technology, bootstrap] : m_defaults) {
        auto upBootstraps = std::make_unique<BootstrapSet>();
        if (!bootstrap.empty()) {
            upBootstraps->emplace(bootstrap);
        }
        m_upEndpointBootstraps->emplace(technology, std::move(upBootstraps));
    }

    Configuration::StatusCode const status = Serialize();
    if (status != Configuration::StatusCode::Success) {
        NodeUtils::printo("Warning: Filed to serialize peers!", NodeUtils::PrintType::Node);
    }

    return status;
}

//-----------------------------------------------------------------------------------------------

void CPeerPersistor::AddBootstrapEntry(
    std::shared_ptr<CBryptPeer> const& spBryptPeer,
    Endpoints::EndpointIdType identifier,
    Endpoints::TechnologyType technology)
{
    // Get the entry from the peer, if there is no entry there is nothing to store. 
    if (auto const optBootstrap = spBryptPeer->GetRegisteredEntry(identifier); optBootstrap) {
        AddBootstrapEntry(technology, *optBootstrap);
    }
}

//-----------------------------------------------------------------------------------------------

void CPeerPersistor::AddBootstrapEntry(
    Endpoints::TechnologyType technology,
    std::string_view bootstrap)
{
    if (bootstrap.empty()) {
        return;
    }

    {
        std::scoped_lock lock(m_endpointBootstrapsMutex);
        // Since we always want ensure the peer can be tracked, we use emplace to either
        // insert a new entry for the technolgoy type or get the existing entry.
        if (auto const [itr, emplaced] = m_upEndpointBootstraps->try_emplace(technology); itr != m_upEndpointBootstraps->end()) {
            auto const& [key, upBootstraps] = *itr;
            if (upBootstraps) {
                upBootstraps->emplace(bootstrap);
            }
        }

        // Write the updated peers to the file
        [[maybe_unused]] auto const status = Serialize();
    }
}

//-----------------------------------------------------------------------------------------------

void CPeerPersistor::DeleteBootstrapEntry(
    std::shared_ptr<CBryptPeer> const& spBryptPeer,
    Endpoints::EndpointIdType identifier,
    Endpoints::TechnologyType technology)
{
    // Get the entry from the peer, if there is no entry there is nothing to delete. 
    if (auto const optBootstrap = spBryptPeer->GetRegisteredEntry(identifier); optBootstrap) {
        DeleteBootstrapEntry(technology, *optBootstrap);
    }
}

//-----------------------------------------------------------------------------------------------

void CPeerPersistor::DeleteBootstrapEntry(
    Endpoints::TechnologyType technology,
    std::string_view bootstrap)
{
    if (bootstrap.empty()) {
        return;
    }

    {
        std::scoped_lock lock(m_endpointBootstrapsMutex);
        // Since we always want ensure the peer can be tracked, we use emplace to either
        // insert a new entry for the technolgoy type or get the existing entry.
        if (auto const [itr, emplaced] = m_upEndpointBootstraps->try_emplace(technology);
            itr != m_upEndpointBootstraps->end()) {
            auto const& [key, upBootstraps] = *itr;
            upBootstraps->erase(bootstrap.data());
        }

        // Write the updated peers to the file
        [[maybe_unused]] auto const status = Serialize();
    }
}

//-----------------------------------------------------------------------------------------------

void CPeerPersistor::HandlePeerStateChange(
    std::weak_ptr<CBryptPeer> const& wpBryptPeer,
    Endpoints::EndpointIdType identifier,
    Endpoints::TechnologyType technology,
    ConnectionState change)
{
    // If the persistor peers have not yet been intialized, simply return
    if (!m_upEndpointBootstraps) {
        return; 
    }

    if (auto const spBryptPeer = wpBryptPeer.lock(); spBryptPeer) {
        switch (change) {
            case ConnectionState::Connected: {
                AddBootstrapEntry(spBryptPeer, identifier, technology);
            } break;
            case ConnectionState::Disconnected:
            case ConnectionState::Flagged: {
                DeleteBootstrapEntry(spBryptPeer, identifier, technology);
            } break;
            default: return; // Currently, we don't persist information for other state changes.
        }
    }
}

//-----------------------------------------------------------------------------------------------

bool CPeerPersistor::ForEachCachedBootstrap(
    AllEndpointBootstrapReadFunction const& callback,
    AllEndpointBootstrapErrorFunction const& error) const
{
    std::scoped_lock lock(m_endpointBootstrapsMutex);
    if (!m_upEndpointBootstraps) {
        return false;
    }

    for (auto const& [technology, upBootstraps]: *m_upEndpointBootstraps) {
        if (!upBootstraps) {
            // Notify the caller that there are no listed peers for an endpoint of a given technology
            error(technology);
            continue;
        }

        CallbackIteration result;
        for (auto const& bootstrap: *upBootstraps) {
            result = callback(technology, bootstrap);
            if (result != CallbackIteration::Continue) {
                break;
            }
        }
        if (result != CallbackIteration::Continue) {
            break;
        }
    }

    return true;
}

//-----------------------------------------------------------------------------------------------

bool CPeerPersistor::ForEachCachedBootstrap(
    Endpoints::TechnologyType technology,
    OneEndpointBootstrapReadFunction const& callback) const
{
    std::scoped_lock lock(m_endpointBootstrapsMutex);
    if (!m_upEndpointBootstraps) {
        return false;
    }

    auto const itr = m_upEndpointBootstraps->find(technology);
    if (itr == m_upEndpointBootstraps->end()) {
        return false;
    }   

    auto const& [key, upBootstraps] = *itr;
    for (auto const& bootstrap: *upBootstraps) {
        auto const result = callback(bootstrap);
        if (result != CallbackIteration::Continue) {
            break;
        }
    }

    return true;
}

//-----------------------------------------------------------------------------------------------

std::uint32_t CPeerPersistor::CachedBootstrapCount() const
{
    std::scoped_lock lock(m_endpointBootstrapsMutex);
    if (!m_upEndpointBootstraps) {
        return 0;
    }

    std::uint32_t count = 0;
    for (auto const& [technology, upBootstraps]: *m_upEndpointBootstraps) {
        if (upBootstraps) {
            count += upBootstraps->size();
        }
    }

    return count;
}

//-----------------------------------------------------------------------------------------------

std::uint32_t CPeerPersistor::CachedBootstrapCount(Endpoints::TechnologyType technology) const
{
    std::scoped_lock lock(m_endpointBootstrapsMutex);
    if (!m_upEndpointBootstraps) {
        return 0;
    }

    if (auto const itr = m_upEndpointBootstraps->find(technology); itr != m_upEndpointBootstraps->end()) {
        auto const& [technology, upBootstraps] = *itr;
        if (upBootstraps) {
            return upBootstraps->size();
        }
    }   
    return 0;
}

//-----------------------------------------------------------------------------------------------

void local::ParseDefaultBootstraps(
    Configuration::EndpointConfigurations const& configurations, 
    CPeerPersistor::DefaultBootstrapMap& defaults)
{
    for (auto const& options: configurations) {
        if (auto const optBootstrap = options.GetBootstrap(); optBootstrap) {
            defaults.emplace(options.type, *optBootstrap);
        }
    }
}

//-----------------------------------------------------------------------------------------------

void local::FillDefaultBootstrap(BootstrapVector& bootstraps, std::string_view target)
{
    if (target.empty()) {
        return;
    }

    bootstraps.emplace_back(target);
}

//-----------------------------------------------------------------------------------------------

void local::WriteEndpointPeers(
    CPeerPersistor::UniqueEndpointBootstrapMap const& upEndpointBootstraps,
    std::ofstream& out)
{
    // If the provided endpoint peers are empty, ther is nothing to do
    if (!upEndpointBootstraps) {
        return;
    }

    std::uint32_t endpointsWritten = 0;
    std::uint32_t const endpointSize = upEndpointBootstraps->size();
    out << "[\n";
    for (auto const& [technology, upBootstraps]: *upEndpointBootstraps) {
        if (technology == Endpoints::TechnologyType::Invalid){
            continue;
        }
        
        out << "\t{\n";
        out << "\t\t\"technology\": \"" << Endpoints::TechnologyTypeToString(technology) << "\",\n";
        out << "\t\t\"bootstraps\": [\n";

        std::uint32_t peersWritten = 0;
        std::uint32_t const peersSize = upBootstraps->size();
        for (auto const& bootstrap: *upBootstraps) {
            out << "\t\t\t{ \"target\": \"" << bootstrap << "\" }";
            if (++peersWritten != peersSize) {
                out << ",\n";
            }
        }

        out << "\n\t\t]\n";
        out << "\t}";
        if (++endpointsWritten != endpointSize) {
            out << ",\n";
        }
    }
    out << "\n]\n";
}

//-----------------------------------------------------------------------------------------------
