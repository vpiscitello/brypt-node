//------------------------------------------------------------------------------------------------
// File: ConfigurationManager.cpp 
// Description:
//-----------------------------------------------------------------------------------------------
#include "Configuration.hpp"
#include "PeerPersistor.hpp"
#include "../Utilities/FileUtils.hpp"
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

void WriteEndpointPeers(
    Configuration::EndpointPeersMap const& endpoints,
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
IOD_SYMBOL(endpoints)
// "endpoints": [
//  {
//     "technology": "Direct",
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
        if (id == 0) {
            return false;
        }

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

Configuration::CPeerPersistor::CPeerPersistor()
    : m_mediator(nullptr)
    , m_filepath()
    , m_endpoints()
{
    m_filepath = GetDefaultPeersFilepath();
    FileUtils::CreateFolderIfNoneExist(m_filepath);
}

//-----------------------------------------------------------------------------------------------

Configuration::CPeerPersistor::CPeerPersistor(
    std::string_view filepath)
    : m_mediator(nullptr)
    , m_filepath(filepath)
    , m_fileMutex()
    , m_endpointsMutex()
    , m_endpoints()
{
    // If the filepath does not have a filename, attach the default config.json
    if (!m_filepath.has_filename()) {
        m_filepath = m_filepath / DefaultConfigurationFilename;
    }

    // If the filepath does not have a parent path, get and attach the default brypt folder
    if (!m_filepath.has_parent_path()) {
        m_filepath = GetDefaultBryptFolder() / m_filepath;
    }

    FileUtils::CreateFolderIfNoneExist(m_filepath);
}

//-----------------------------------------------------------------------------------------------

void Configuration::CPeerPersistor::SetMediator(IPeerMediator* const mediator)
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

Configuration::OptionalEndpointPeersMap Configuration::CPeerPersistor::FetchPeers()
{
    StatusCode status;
    if (std::filesystem::exists(m_filepath)) {
        NodeUtils::printo(
            "Reading peers file at: " + m_filepath.string(),
            NodeUtils::PrintType::Node);
        status = DecodePeersFile();
    }

    if (status != StatusCode::Success) {
        NodeUtils::printo(
            "Failed to decode peers file at: " + m_filepath.string(),
            NodeUtils::PrintType::Node);
        return {};
    }

    return m_endpoints;
}

//-----------------------------------------------------------------------------------------------

Configuration::StatusCode Configuration::CPeerPersistor::Serialize()
{
    std::scoped_lock lock(m_fileMutex);
    
    if (m_endpoints.empty()) {
        return StatusCode::InputError;
    }

    if (m_filepath.empty()) {
        return StatusCode::FileError;
    }

    std::ofstream out(m_filepath, std::ofstream::out);
    if (out.fail()) {
        return StatusCode::FileError;
    }

    out << "{\n";

    local::WriteEndpointPeers(m_endpoints, out);

    out << "}" << std::flush;

    out.close();

    return StatusCode::Success;
}

//-----------------------------------------------------------------------------------------------

Configuration::StatusCode Configuration::CPeerPersistor::DecodePeersFile()
{
    // Determine the size of the file about to be read. Do not read a file
    // that is above the given treshold. 
    std::error_code error;
    auto const size = std::filesystem::file_size(m_filepath, error);
    if (error || size == 0 || size > defaults::FileSizeLimit) {
        return StatusCode::FileError;
    }

    // Attempt to open the configuration file, if it fails return noopt
    std::ifstream file(m_filepath);
    if (file.fail()) {
        return StatusCode::FileError;
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

    auto const& endpointEntries = knownPeersEntry.endpoints;    
    m_endpoints.clear();
    m_endpoints.reserve(endpointEntries.size());

    std::for_each(
        endpointEntries.begin(), endpointEntries.end(),
        [&] (local::TEndpointEntry const& endpointEntry)
        {
            // Parse the technology name from the entry, if it is not a valid name continue to 
            // the next endpoint entry.
            auto const technology = Endpoints::ParseTechnologyType(endpointEntry.technology);
            if (technology == Endpoints::TechnologyType::Invalid) {
                return;
            }

            auto& peerEntries = endpointEntry.peers;
            auto const [itr, inserted] = m_endpoints.emplace(technology, PeersMap{});
            auto& peers = itr->second;
            peers.reserve(peerEntries.size() * 2);

            if (peerEntries.empty()) {
                NodeUtils::printo(
                    "Warning: the entry for \"" + endpointEntry.technology + "\" has no listed bootstrap peer(s)",
                    NodeUtils::PrintType::Node);
                return;
            }

            // Insert any valid peers into the endpoint map
            std::for_each(
                peerEntries.begin(), peerEntries.end(),
                [&technology, &peers] (local::TPeerEntry const& peerEntry)
                {
                    if (!peerEntry.IsValid()) {
                        return;
                    }

                    peers.try_emplace(
                        // Peer Key
                        peerEntry.id,
                        // Peer Class
                        peerEntry.id, 
                        technology,
                        peerEntry.entry,
                        peerEntry.location);
                }
            );
        }
    );

    if (m_endpoints.empty()) {
        return StatusCode::DecodeError;
    }

    // Valdate the decoded settings. If the settings decoded are not valid return noopt.
    return StatusCode::Success;
}

//-----------------------------------------------------------------------------------------------

//-----------------------------------------------------------------------------------------------
// Description: Generates a new configuration file based on user command line arguements or
// from user input.
//-----------------------------------------------------------------------------------------------
Configuration::StatusCode Configuration::CPeerPersistor::SerializeEndpointPeers()
{
    if (m_endpoints.empty()) {
        return StatusCode::DecodeError;
    }

    for (auto const& [endpoint, peers] : m_endpoints) {
        if (peers.empty()) {
            auto const technology = Endpoints::TechnologyTypeToString(endpoint);
            NodeUtils::printo(
                "Warning: " + technology + " has no attached bootstrap peers!",
                NodeUtils::PrintType::Node);
        }
    }

    StatusCode const status = Serialize();
    if (status != StatusCode::Success) {
        NodeUtils::printo(
            "Failed to save configuration settings to: " + m_filepath.string(),
            NodeUtils::PrintType::Node);
    }

    return status;
}

//-----------------------------------------------------------------------------------------------

Configuration::OptionalEndpointPeersMap Configuration::CPeerPersistor::GetCachedPeers() const
{
    return m_endpoints;
}

//-----------------------------------------------------------------------------------------------

Configuration::OptionalPeersMap Configuration::CPeerPersistor::GetCachedPeers(
    Endpoints::TechnologyType technology) const
{
    if (auto const itr = m_endpoints.find(technology); itr != m_endpoints.end()) {
        return itr->second;
    }
    return {};
}

//-----------------------------------------------------------------------------------------------

void Configuration::CPeerPersistor::HandlePeerConnectionStateChange(
    CPeer const& peer,
    ConnectionState change)
{
    switch (change) {
        case ConnectionState::Connected: {
            std::scoped_lock lock(m_endpointsMutex);
            // Since we always want ensure the peer can be tracked, we use emplace to either
            // insert a new entry for the technolgoy type or get the existing entry.
            auto const [itr, emplaced] = m_endpoints.try_emplace(peer.GetTechnologyType());
            if (itr != m_endpoints.end()) {
                // TODO: What should happen if the peer is already in the map?
                itr->second.try_emplace(peer.GetNodeId(), peer);
            } 
        } break;
        case ConnectionState::Disconnected:
        case ConnectionState::Flagged: {
            std::scoped_lock lock(m_endpointsMutex);
            if (auto const itr = m_endpoints.find(peer.GetTechnologyType());
                itr != m_endpoints.end()) {
                itr->second.erase(peer.GetNodeId()); // Remove the provided peer
            }
        } break;
        default: return; // Currently, we don't adjust the peers for any other changes.
    }

    auto const status = Serialize(); // Write the updated peers to the file
    if (status != StatusCode::Success) {
        auto const technology = Endpoints::TechnologyTypeToString(peer.GetTechnologyType());
        NodeUtils::printo(
            "Warning: Filed to serialize peers for " + technology + " endpoint!",
            NodeUtils::PrintType::Node);
    }
}

//-----------------------------------------------------------------------------------------------

void local::WriteEndpointPeers(
    Configuration::EndpointPeersMap const& endpoints,
    std::ofstream& out)
{
    std::uint32_t endpointsWritten = 0;
    std::uint32_t const endpointSize = endpoints.size();
    out << "\t\"endpoints\": [\n";
    for (auto const& [technology, peers]: endpoints) {
        out << "\t\t{\n";
        out << "\t\t\t\"technology\": \"" << Endpoints::TechnologyTypeToString(technology) << "\",\n";
        out << "\t\t\t\"peers\": [\n";

        std::uint32_t peersWritten = 0;
        std::uint32_t const peersSize = peers.size();
        for (auto const& [id, peer]: peers) {
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
