//------------------------------------------------------------------------------------------------
// File: PeerDetailsMap.hpp
// Description: A template class for connection types to store peer information. Allows
// translation between internal connection identifiers (i.e. IPv4 addresses) and Brypt Node IDs.
// Peer connection state information will also be managed by this class (i.e. connection state).
//------------------------------------------------------------------------------------------------
#pragma once
//------------------------------------------------------------------------------------------------
#include "PeerDetails.hpp"
#include "../../Utilities/CallbackIteration.hpp"
#include "../../Utilities/NodeUtils.hpp"
//------------------------------------------------------------------------------------------------
#include <functional>
#include <optional>
#include <unordered_map>
//------------------------------------------------------------------------------------------------


//------------------------------------------------------------------------------------------------

struct ConnectionIdHasher {
    template <typename ConnectionIdType>
    std::size_t operator()(ConnectionIdType const& id) const {
        return std::hash<ConnectionIdType>()(id);
    }
};

//------------------------------------------------------------------------------------------------

template <typename ConnectionIdType, typename ExtensionType = void>
class CPeerInformationMap
{
public:
    using ReadOneFunction = std::function<void(CPeerDetails<ExtensionType> const&)>;
    using UpdateOneFunction = std::function<void(CPeerDetails<ExtensionType>&)>;
    using ReadMultipleFunction = std::function<CallbackIteration(ConnectionIdType const&, CPeerDetails<ExtensionType> const&)>;
    using UpdateMultipleFunction = std::function<CallbackIteration(ConnectionIdType const&, CPeerDetails<ExtensionType>&)>;

    CPeerInformationMap()
        : m_peers()
        , m_nodeIdLookups()
    {
    };

    ~CPeerInformationMap()
    {
    };

//------------------------------------------------------------------------------------------------

    void TrackNode(
        ConnectionIdType const& connectionId,
        CPeerDetails<ExtensionType> const& details)
    {
        // Attempt to find a mapping for the connection ID, if it is found the node is 
        // already being tracked and we should return. The internal maps need to be kept in
        // sync for this container to operate as expected and therefor we only need to check
        // one map.
        auto const connectionIdMappingItr = m_peers.find(connectionId);
        if (connectionIdMappingItr != m_peers.end()) {
            return;
        }

        auto const itr = m_peers.emplace(connectionId, details);
        m_nodeIdLookups.emplace(details.GetNodeId(), itr.first);
    }

//------------------------------------------------------------------------------------------------

    void UntrackNode(ConnectionIdType connectionId)
    {
        auto const itr = m_peers.find(connectionId);
        if (itr == m_peers.end()) {
            return;
        }
        m_nodeIdLookups.erase(itr->second.GetNodeId());
        m_peers.erase(itr);
    }

//------------------------------------------------------------------------------------------------

    bool UpdateOneNode(
        ConnectionIdType const& id, UpdateOneFunction const& updateFunction)
    {
        auto const itr = m_peers.find(id);
        if (itr == m_peers.end()) {
            return false;
        }
        updateFunction(itr->second);
        return true;
    }

//------------------------------------------------------------------------------------------------

    bool ReadOneNode(
        ConnectionIdType const& id, ReadOneFunction const& readFunction)
    {
        auto const itr = m_peers.find(id);
        if (itr == m_peers.end()) {
            return false;
        }
        readFunction(itr->second);
        return true;
    }

//------------------------------------------------------------------------------------------------

    void UpdateEachNode(UpdateMultipleFunction const& updateFunction)
    {
        for (auto& [id, details] : m_peers) {
            auto const result = updateFunction(id, details);
            if (result == CallbackIteration::Stop) {
                return;
            }
        }
    }

//------------------------------------------------------------------------------------------------

    void ReadEachNode(ReadMultipleFunction const& readFunction)
    {
        for (auto const& [id, details] : m_peers) {
            auto const result = readFunction(id, details);
            if (result == CallbackIteration::Stop) {
                return;
            }
        }
    }

//------------------------------------------------------------------------------------------------

    std::optional<NodeUtils::NodeIdType> Translate(ConnectionIdType const& id)
    {
        auto const itr = m_peers.find(id);
        if (itr == m_peers.end()) {
            return {};
        }
        return itr->second.GetNodeId();
    }

//------------------------------------------------------------------------------------------------

    std::optional<ConnectionIdType> Translate(NodeUtils::NodeIdType id)
    {
        auto const itr = m_nodeIdLookups.find(id);
        if (itr == m_nodeIdLookups.end()) {
            return {};
        }
        return itr->second->first;
    }

//------------------------------------------------------------------------------------------------

private:
    using PeerDetailsMap =
         typename std::unordered_map<ConnectionIdType, CPeerDetails<ExtensionType>, ConnectionIdHasher>;
    using PeerDetailsLookup = typename PeerDetailsMap::iterator;
    
    using NodeIdToPeerLookupMap = 
        std::unordered_map<NodeUtils::NodeIdType, PeerDetailsLookup>;

    PeerDetailsMap m_peers; // An unordered map from connection IDs to connection details
    NodeIdToPeerLookupMap m_nodeIdLookups; // An unordered map from node ID to connection details lookup
};

//------------------------------------------------------------------------------------------------