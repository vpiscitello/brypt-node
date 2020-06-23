//------------------------------------------------------------------------------------------------
// File: ConfigurationManager.cpp 
// Description:
//-----------------------------------------------------------------------------------------------
#include "Configuration.hpp"
#include "PeerPersistor.hpp"
#include "../Utilities/FileUtils.hpp"
#include "../Utilities/ReservedIdentifiers.hpp"
//-----------------------------------------------------------------------------------------------
#include "../Libraries/metajson/metajson.hh"
#include <cstdlib>
#include <iostream>
#include <fstream>
#include <vector>
//-----------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
namespace {
namespace local {
//------------------------------------------------------------------------------------------------

struct TPeerEntry;
struct TEndpointEntry;
struct TKnownPeersEntry;

using PeerEntriesVector = std::vector<TPeerEntry>;
using EndpointEntriesVector = std::vector<TEndpointEntry>;

void FillDefaultBootstrap(Endpoints::TechnologyType technology, PeerEntriesVector& entries);

void WriteEndpointPeers(
    CPeerPersistor::SharedEndpointPeersMap const& endpoints,
    std::ofstream& out);

//------------------------------------------------------------------------------------------------
} // local namespace
//------------------------------------------------------------------------------------------------
namespace defaults {
//------------------------------------------------------------------------------------------------

constexpr std::uint32_t FileSizeLimit = 12'000; // Limit the peers files to 12KB

constexpr std::string_view TcpEntry = "127.0.0.1:35216";
constexpr std::string_view LoRaEntry = "915:7";

//------------------------------------------------------------------------------------------------
} // default namespace
} // namespace
//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
// Description: Symbol loading for JSON encoding. 
//------------------------------------------------------------------------------------------------
IOD_SYMBOL(endpoints)
// "endpoints": [
//  {
//     "technology": String,
//     "peers": [
//      {
//         "id": Number
//         "entry": String,
//         "location": String
//     },
//     ...
//     ],
// },
// ...,
// ]

IOD_SYMBOL(entry)
IOD_SYMBOL(technology)
IOD_SYMBOL(id)
IOD_SYMBOL(location)
IOD_SYMBOL(peers)

//------------------------------------------------------------------------------------------------

struct local::TPeerEntry
{
    TPeerEntry()
        : id()
        , entry()
        , location()
    {
    }

    TPeerEntry(
        NodeUtils::NodeIdType id,
        std::string_view entry,
        std::string_view location)
        : id(id)
        , entry(entry)
        , location(location)
    {
    }

    bool IsValid() const
    {
        // TODO: Implement invalid node ID 
        // if (id == 0) {
        //     return false;
        // }

        if (entry.empty()) {
            return false;
        }

        return true;
    }

    NodeUtils::NodeIdType id;
    std::string entry;
    std::string location;
};

//-----------------------------------------------------------------------------------------------

struct local::TEndpointEntry
{    
    TEndpointEntry()
        : technology()
        , peers()
    {
    }

    TEndpointEntry(
        std::string_view technology,
        std::vector<TPeerEntry> const& peers)
        : technology(technology)
        , peers(peers)
    {
    }

    std::string technology;
    std::vector<TPeerEntry> peers;
};

//-----------------------------------------------------------------------------------------------

struct local::TKnownPeersEntry
{
    TKnownPeersEntry()
        : endpoints()
    {
    }

    TKnownPeersEntry(std::vector<TEndpointEntry> const& endpoints)
        : endpoints(endpoints)
    {
    }

    EndpointEntriesVector endpoints;
};

//-----------------------------------------------------------------------------------------------

CPeerPersistor::CPeerPersistor()
    : m_mediator(nullptr)
    , m_filepath()
    , m_endpointsMutex()
    , m_spEndpoints()
{
    m_filepath = Configuration::GetDefaultPeersFilepath();
    FileUtils::CreateFolderIfNoneExist(m_filepath);
}

//-----------------------------------------------------------------------------------------------

CPeerPersistor::CPeerPersistor(
    std::string_view filepath)
    : m_mediator(nullptr)
    , m_filepath(filepath)
    , m_fileMutex()
    , m_endpointsMutex()
    , m_spEndpoints()
{
    // If the filepath does not have a filename, attach the default config.json
    if (!m_filepath.has_filename()) {
        m_filepath = m_filepath / Configuration::DefaultConfigurationFilename;
    }

    // If the filepath does not have a parent path, get and attach the default brypt folder
    if (!m_filepath.has_parent_path()) {
        m_filepath = Configuration::GetDefaultBryptFolder() / m_filepath;
    }

    FileUtils::CreateFolderIfNoneExist(m_filepath);
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

bool CPeerPersistor::FetchPeers()
{
    Configuration::StatusCode status;
    if (std::filesystem::exists(m_filepath)) {
        NodeUtils::printo(
            "Reading peers file at: " + m_filepath.string(),
            NodeUtils::PrintType::Node);
        status = DecodePeersFile();
    }

    if (!m_spEndpoints || status != Configuration::StatusCode::Success) {
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
    
    if (!m_spEndpoints) {
        return Configuration::StatusCode::InputError;
    }

    if (m_filepath.empty()) {
        return Configuration::StatusCode::FileError;
    }

    std::ofstream out(m_filepath, std::ofstream::out);
    if (out.fail()) {
        return Configuration::StatusCode::FileError;
    }

    out << "{\n";

    local::WriteEndpointPeers(m_spEndpoints, out);

    out << "}" << std::flush;

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

    local::TKnownPeersEntry knownPeersEntry;

    // Decode the JSON string into the configuration struct
    iod::json_object(
        s::endpoints = iod::json_vector(
            s::technology,
            s::peers = iod::json_vector(
                s::id,
                s::entry,
                s::location
            ))
        ).decode(json, knownPeersEntry);

    auto& endpointEntries = knownPeersEntry.endpoints;
    auto spEndpoints = std::make_shared<EndpointPeersMap>();
    spEndpoints->reserve(endpointEntries.size());

    std::for_each(
        endpointEntries.begin(), endpointEntries.end(),
        [&] (local::TEndpointEntry& endpointEntry)
        {
            // Parse the technology name from the entry, if it is not a valid name continue to 
            // the next endpoint entry.
            auto const technology = Endpoints::ParseTechnologyType(endpointEntry.technology);
            if (technology == Endpoints::TechnologyType::Invalid) {
                return;
            }

            auto& peerEntries = endpointEntry.peers;
            auto const spPeers = std::make_shared<PeersMap>();
            spPeers->reserve(peerEntries.size() * 2);

            if (peerEntries.empty()) {
                NodeUtils::printo(
                    "Warning: The endpoint \"" + endpointEntry.technology + "\" has no listed bootstrap peer(s)!" + 
                    "Using default bootstrap entry point for the endpoint, custom bootstraps may be set in the peers.json file.",
                    NodeUtils::PrintType::Node);
                local::FillDefaultBootstrap(technology, peerEntries);
            }

            // Insert any valid peers into the endpoint map
            std::for_each(
                peerEntries.begin(), peerEntries.end(),
                [&technology, &spPeers] (local::TPeerEntry const& peerEntry)
                {
                    if (!peerEntry.IsValid()) {
                        return;
                    }

                    spPeers->try_emplace(
                        // Peer Key
                        peerEntry.id,
                        // Peer Class
                        peerEntry.id, 
                        technology,
                        peerEntry.entry,
                        peerEntry.location);
                }
            );
            
            spEndpoints->emplace(technology, spPeers);
        }
    );

    if (spEndpoints->empty()) {
        return Configuration::StatusCode::DecodeError;
    }

    m_spEndpoints = spEndpoints;
    return Configuration::StatusCode::Success;
}

//-----------------------------------------------------------------------------------------------

//-----------------------------------------------------------------------------------------------
// Description: Generates a new configuration file based on user command line arguements or
// from user input.
//-----------------------------------------------------------------------------------------------
Configuration::StatusCode CPeerPersistor::SerializeEndpointPeers()
{
    if (!m_spEndpoints) {
        return Configuration::StatusCode::DecodeError;
    }

    for (auto const& [endpoint, spPeers] : *m_spEndpoints) {
        if (spPeers->empty()) {
            auto const technology = Endpoints::TechnologyTypeToString(endpoint);
            NodeUtils::printo(
                "Warning: " + technology + " has no attached bootstrap peers!",
                NodeUtils::PrintType::Node);
        }
    }

    Configuration::StatusCode const status = Serialize();
    if (status != Configuration::StatusCode::Success) {
        NodeUtils::printo(
            "Failed to save configuration settings to: " + m_filepath.string(),
            NodeUtils::PrintType::Node);
    }

    return status;
}

//-----------------------------------------------------------------------------------------------

bool CPeerPersistor::ForEachCachedEndpoint(AllEndpointReadFunction const& readFunction) const
{
    std::scoped_lock lock(m_endpointsMutex);
    if (!m_spEndpoints) {
        return false;
    }

    for (auto const& [technology, spPeersMap]: *m_spEndpoints) {
        if (readFunction(technology) != CallbackIteration::Continue) {
            return false;
        }
    }

    return true;  
}

//-----------------------------------------------------------------------------------------------

bool CPeerPersistor::ForEachCachedPeer(
    AllEndpointPeersReadFunction const& readFunction,
    AllEndpointPeersErrorFunction const& errorFunction) const
{
    std::scoped_lock lock(m_endpointsMutex);
    if (!m_spEndpoints) {
        return false;
    }

    for (auto const& [technology, spPeersMap]: *m_spEndpoints) {
        if (!spPeersMap) {
            // Notify the caller that there are no listed peers for an endpoint of a given technology
            errorFunction(technology);
            continue;
        }

        CallbackIteration result;
        for (auto const& [id, peer]: *spPeersMap) {
            result = readFunction(technology, peer);
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

bool CPeerPersistor::ForEachCachedPeer(
    Endpoints::TechnologyType technology,
    OneEndpointPeersReadFunction const& readFunction) const
{
    std::scoped_lock lock(m_endpointsMutex);
    if (!m_spEndpoints) {
        return false;
    }

    auto const itr = m_spEndpoints->find(technology);
    if (itr == m_spEndpoints->end()) {
        return false;
    }   

    auto const& [key, spPeersMap] = *itr;
    for (auto const& [id, peer]: *spPeersMap) {
        auto const result = readFunction(peer);
        if (result != CallbackIteration::Continue) {
            break;
        }
    }

    return true;
}

//-----------------------------------------------------------------------------------------------

std::uint32_t CPeerPersistor::CachedEndpointsCount() const
{
    std::scoped_lock lock(m_endpointsMutex);
    if (m_spEndpoints) {
        return m_spEndpoints->size();
    }
    return 0;
}

//-----------------------------------------------------------------------------------------------

std::uint32_t CPeerPersistor::CachedPeersCount() const
{
    std::scoped_lock lock(m_endpointsMutex);
    if (!m_spEndpoints) {
        return 0;
    }

    std::uint32_t count = 0;
    for (auto const& [technology, spPeersMap]: *m_spEndpoints) {
        if (spPeersMap) {
            count += spPeersMap->size();
        }
    }

    return count;
}

//-----------------------------------------------------------------------------------------------

std::uint32_t CPeerPersistor::CachedPeersCount(Endpoints::TechnologyType technology) const
{
    std::scoped_lock lock(m_endpointsMutex);
    if (!m_spEndpoints) {
        return 0;
    }

    if (auto const itr = m_spEndpoints->find(technology); itr != m_spEndpoints->end()) {
        auto const& [technology, spPeersMap] = *itr;
        if (spPeersMap) {
            return spPeersMap->size();
        }
    }   
    return 0;
}

//-----------------------------------------------------------------------------------------------

void CPeerPersistor::HandlePeerConnectionStateChange(
    CPeer const& peer,
    ConnectionState change)
{
    // If the persistor peers have not yet been intialized, simply return
    if (!m_spEndpoints) {
        return; 
    }

    {
        std::scoped_lock lock(m_endpointsMutex);
        switch (change) {
            case ConnectionState::Connected: {
                // Since we always want ensure the peer can be tracked, we use emplace to either
                // insert a new entry for the technolgoy type or get the existing entry.
                auto const [itr, emplaced] = m_spEndpoints->try_emplace(peer.GetTechnologyType());
                if (itr != m_spEndpoints->end()) {
                    // TODO: What should happen if the peer is already in the map?
                    itr->second->try_emplace(peer.GetNodeId(), peer);
                } 
            } break;
            case ConnectionState::Disconnected:
            case ConnectionState::Flagged: {
                if (auto const itr = m_spEndpoints->find(peer.GetTechnologyType());
                    itr != m_spEndpoints->end()) {
                    itr->second->erase(peer.GetNodeId()); // Remove the provided peer
                }
            } break;
            default: return; // Currently, we don't adjust the peers for any other changes.
        }
    }

    auto const status = Serialize(); // Write the updated peers to the file
    if (status != Configuration::StatusCode::Success) {
        auto const technology = Endpoints::TechnologyTypeToString(peer.GetTechnologyType());
        NodeUtils::printo(
            "Warning: Filed to serialize peers for " + technology + " endpoint!",
            NodeUtils::PrintType::Node);
    }
}

//-----------------------------------------------------------------------------------------------

void local::FillDefaultBootstrap(
    Endpoints::TechnologyType technology,
    PeerEntriesVector& entries)
{
    TPeerEntry bootstrap = {
        static_cast<NodeUtils::NodeIdType>(ReservedIdentifiers::Unknown),
        "", ""};

    switch (technology) {
        case Endpoints::TechnologyType::Direct:
        case Endpoints::TechnologyType::StreamBridge:
        case Endpoints::TechnologyType::TCP: {
            bootstrap.entry = defaults::TcpEntry;
        } break;
        case Endpoints::TechnologyType::LoRa: {
            bootstrap.entry = defaults::LoRaEntry;
        } break;
        default: break; // No other technology types have a default entry point
    }

    entries.emplace_back(bootstrap);
}

//-----------------------------------------------------------------------------------------------

void local::WriteEndpointPeers(
    CPeerPersistor::SharedEndpointPeersMap const& spEndpoints,
    std::ofstream& out)
{
    // If the provided endpoint peers are empty, ther is nothing to do
    if (!spEndpoints) {
        return;
    }

    std::uint32_t endpointsWritten = 0;
    std::uint32_t const endpointSize = spEndpoints->size();
    out << "\t\"endpoints\": [\n";
    for (auto const& [technology, spPeers]: *spEndpoints) {
        out << "\t\t{\n";
        out << "\t\t\t\"technology\": \"" << Endpoints::TechnologyTypeToString(technology) << "\",\n";
        out << "\t\t\t\"peers\": [\n";

        std::uint32_t peersWritten = 0;
        std::uint32_t const peersSize = spPeers->size();
        for (auto const& [id, peer]: *spPeers) {
            out << "\t\t\t\t{\n";
            out << "\t\t\t\t\t\"id\": " << peer.GetNodeId() << ",\n";
            out << "\t\t\t\t\t\"entry\": \"" << peer.GetEntry() << "\",\n";
            out << "\t\t\t\t\t\"location\": \"" << peer.GetLocation() << "\"\n";
            out << "\t\t\t\t}";
            if (++peersWritten != peersSize) {
                out << ",\n";
            }
        }

        out << "\n\t\t\t]\n";
        out << "\t\t}";
        if (++endpointsWritten != endpointSize) {
            out << ",\n";
        }
    }
    out << "\n\t]\n";
}

//-----------------------------------------------------------------------------------------------
