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
#include "../../Utilities/EnumMaskUtils.hpp"
#include "../../Utilities/NodeUtils.hpp"
//------------------------------------------------------------------------------------------------
#include <array>
#include <cassert>
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

template <typename CopyType>
using CopyFilterPredicate = std::function<bool(CopyType)>;

template <typename ConstReferenceType>
using ConstReferencePredicate = std::function<bool(ConstReferenceType const&)>;

template <typename PredicateType>
using FilterPredicate = std::conditional_t<
                            sizeof(PredicateType) <= sizeof(std::uint32_t),
                            CopyFilterPredicate<PredicateType>,
                            ConstReferencePredicate<PredicateType>>;

//------------------------------------------------------------------------------------------------

enum class ConnectionStateFilter : std::uint8_t {
    Connected = MaskLevel(std::uint8_t(0)),
    Disconnected = MaskLevel(std::uint8_t(1)),
    Flagged = MaskLevel(std::uint8_t(2)),
    Resolving = MaskLevel(std::uint8_t(3)),
    Unknown = MaskLevel(std::uint8_t(4)),
    Invalid = MaskLevel(std::uint8_t(5))
};

ENABLE_ENUM_MASKING(ConnectionStateFilter)

//------------------------------------------------------------------------------------------------

enum class MessageSequenceFilter : std::uint8_t {
    MatchPredicate
};

//------------------------------------------------------------------------------------------------

enum class PromotionStateFilter : std::uint8_t {
    Unpromoted,
    Promoted
};

//------------------------------------------------------------------------------------------------

enum class UpdateTimepointFilter : std::uint8_t {
    MatchPredicate
};

//------------------------------------------------------------------------------------------------

inline ConnectionStateFilter ConnectionStateToFilter(ConnectionState state)
{
    static const std::unordered_map<ConnectionState, ConnectionStateFilter> equivalents = 
    {
        { ConnectionState::Connected, ConnectionStateFilter::Connected },
        { ConnectionState::Disconnected, ConnectionStateFilter::Disconnected },
        { ConnectionState::Flagged, ConnectionStateFilter::Flagged },
        { ConnectionState::Resolving, ConnectionStateFilter::Resolving},
        { ConnectionState::Unknown, ConnectionStateFilter::Unknown }
    };

    if (auto const itr = equivalents.find(state); itr != equivalents.end()) {
        return itr->second;
    }

    return ConnectionStateFilter::Invalid;
}

//------------------------------------------------------------------------------------------------

template <typename ConnectionIdType, typename ExtensionType = void>
class CPeerInformationMap
{
public:

    using ExtendedPeerDetails = CPeerDetails<ExtensionType>;
    using OptionalPeerDetails = std::optional<ExtendedPeerDetails>;
    
    using ForEachFunction = std::function<CallbackIteration(ConnectionIdType const&)>;

    using ReadOneFunction = std::function<void(ExtendedPeerDetails const&)>;
    using UpdateOneFunction = std::function<void(ExtendedPeerDetails&)>;
    using ReadMultipleFunction = std::function<CallbackIteration(ConnectionIdType const&, OptionalPeerDetails const&)>;
    using UpdateMultipleFunction = std::function<CallbackIteration(ConnectionIdType const&, OptionalPeerDetails&)>;

    CPeerInformationMap()
        : m_mutex()
        , m_peers()
        , m_nodeIdLookups()
    {
    };

    //------------------------------------------------------------------------------------------------

    void TrackConnection(
        ConnectionIdType const& connectionId,
        OptionalPeerDetails const& optDetails = {})
    {
        std::scoped_lock lock(m_mutex);
        // Attempt to find a mapping for the connection ID, if it is found the node is 
        // already being tracked and we should return. The internal maps need to be kept in
        // sync for this container to operate as expected and therefor we only need to check
        // one map.
        auto const foundIterator = m_peers.find(connectionId);
        if (foundIterator != m_peers.end()) {
            return;
        }

        auto const [itr, result] = m_peers.emplace(connectionId, optDetails);

        if (result && optDetails) {
            m_nodeIdLookups.emplace(optDetails->GetNodeId(), itr);
        }
    }

    //------------------------------------------------------------------------------------------------

    bool PromoteConnection(
        ConnectionIdType const& connectionId,
        ExtendedPeerDetails const& details)
    {
        std::scoped_lock lock(m_mutex);
        auto const itr = m_peers.find(connectionId);
        if (itr == m_peers.end()) {
            return false;
        }

        itr->second = std::move(details);
        m_nodeIdLookups.emplace(details.GetNodeId(), itr);

        return true;
    }

    //------------------------------------------------------------------------------------------------

    void UntrackConnection(ConnectionIdType connectionId)
    {
        std::scoped_lock lock(m_mutex);
        auto const itr = m_peers.find(connectionId);
        if (itr == m_peers.end()) {
            return;
        }

        OptionalPeerDetails const& optDetails = itr->second;
        if (optDetails) {
            m_nodeIdLookups.erase(optDetails->GetNodeId());
        }

        m_peers.erase(itr);
    }

    //------------------------------------------------------------------------------------------------

    bool UpdateOnePeer(ConnectionIdType const& id, UpdateOneFunction const& updateFunction)
    {
        std::scoped_lock lock(m_mutex);
        auto const itr = m_peers.find(id);
        if (itr == m_peers.end()) {
            return false;
        }

        OptionalPeerDetails& optDetails = itr->second;
        if (!optDetails) {
            return false;
        }

        updateFunction(*optDetails);
        return true;
    }

    //------------------------------------------------------------------------------------------------

    void ForEachConnection(ForEachFunction const& forEachFunction)
    {
        std::scoped_lock lock(m_mutex);
        for (auto& [id, optDetails] : m_peers) {
            auto const result = forEachFunction(id);
            if (result == CallbackIteration::Stop) {
                return;
            }
        }
    }

    //------------------------------------------------------------------------------------------------

    bool ReadOnePeer(ConnectionIdType const& id, ReadOneFunction const& readFunction)
    {
        std::scoped_lock lock(m_mutex);
        auto const itr = m_peers.find(id);
        if (itr == m_peers.end()) {
            return false;
        }

        OptionalPeerDetails const& optDetails = itr->second;
        if (!optDetails) {
            return false;
        }

        readFunction(*optDetails);
        return true;
    }

    //------------------------------------------------------------------------------------------------

    void UpdateEachPeer(UpdateMultipleFunction const& updateFunction)
    {
        std::scoped_lock lock(m_mutex);
        for (auto& [id, optDetails] : m_peers) {
            auto const result = updateFunction(id, optDetails);
            if (result == CallbackIteration::Stop) {
                return;
            }
        }
    }

    //------------------------------------------------------------------------------------------------

    void UpdateEachPeer(
        UpdateMultipleFunction const& updateFunction,
        ConnectionStateFilter filter)
    {
        std::scoped_lock lock(m_mutex);
        for (auto& [id, optDetails] : m_peers) {
            if (!optDetails) {
                continue;
            }

            auto const peerConnectionState = optDetails->GetConnectionState();
            auto const stateFilterEquivalent = ConnectionStateToFilter(peerConnectionState);

            CallbackIteration result = CallbackIteration::Continue;
            if (FlagIsSet(stateFilterEquivalent, filter)) {
                result = updateFunction(id, optDetails);
            }
            
            if (result == CallbackIteration::Stop) {
                return;
            }
        }
    }

    //------------------------------------------------------------------------------------------------

    void UpdateEachPeer(
        UpdateMultipleFunction const& updateFunction,
        [[maybe_unused]] MessageSequenceFilter filter,
        FilterPredicate<std::uint32_t> const& predicate)
    {
        std::scoped_lock lock(m_mutex);
        for (auto& [id, optDetails] : m_peers) {
            if (!optDetails) {
                continue;
            }

            auto const sequence = optDetails->GetMessageSequenceNumber();

            CallbackIteration result = CallbackIteration::Continue;
            if (predicate(sequence)) {
                result = updateFunction(id, optDetails);
            }
            
            if (result == CallbackIteration::Stop) {
                return;
            }
        }
    }

    //------------------------------------------------------------------------------------------------

    void UpdateEachPeer(
        UpdateMultipleFunction const& updateFunction,
        PromotionStateFilter filter)
    {
        std::scoped_lock lock(m_mutex);
        for (auto& [id, optDetails] : m_peers) {
            CallbackIteration result = CallbackIteration::Continue;
            switch (filter) {
                case PromotionStateFilter::Promoted: {
                    if (optDetails) {
                        result = updateFunction(id, optDetails);
                    }
                } break;
                case PromotionStateFilter::Unpromoted: {
                    if (!optDetails) {
                        result = updateFunction(id, optDetails);
                    }
                } break;
                default: assert(true); break; // What is this?
            }
            
            if (result == CallbackIteration::Stop) {
                return;
            }
        }
    }

    //------------------------------------------------------------------------------------------------

    void UpdateEachPeer(
        UpdateMultipleFunction const& updateFunction,
        [[maybe_unused]] UpdateTimepointFilter filter,
        FilterPredicate<NodeUtils::Timepoint> const& predicate)
    {
        std::scoped_lock lock(m_mutex);
        for (auto& [id, optDetails] : m_peers) {
            if (!optDetails) {
                continue;
            }

            auto const timepoint = optDetails->GetUpdateTimepoint();

            CallbackIteration result = CallbackIteration::Continue;
            if (predicate(timepoint)) {
                result = updateFunction(id, optDetails);
            }
            
            if (result == CallbackIteration::Stop) {
                return;
            }
        }
    }

    //------------------------------------------------------------------------------------------------

    void ReadEachPeer(ReadMultipleFunction const& readFunction)
    {
        std::scoped_lock lock(m_mutex);
        for (auto const& [id, optDetails] : m_peers) {
            auto const result = readFunction(id, optDetails);
            if (result == CallbackIteration::Stop) {
                return;
            }
        }
    }

    //------------------------------------------------------------------------------------------------

    void ReadEachPeer(
        ReadMultipleFunction const& readFunction,
        ConnectionStateFilter filter)
    {
        std::scoped_lock lock(m_mutex);
        for (auto const& [id, optDetails] : m_peers) {
            if (!optDetails) {
                continue;
            }

            auto const peerConnectionState = optDetails->GetConnectionState();
            auto const stateFilterEquivalent = ConnectionStateToFilter(peerConnectionState);

            CallbackIteration result = CallbackIteration::Continue;
            if (FlagIsSet(stateFilterEquivalent, filter)) {
                result = readFunction(id, optDetails);
            }
            
            if (result == CallbackIteration::Stop) {
                return;
            }
        }
    }

    //------------------------------------------------------------------------------------------------

    void ReadEachPeer(
        ReadMultipleFunction const& readFunction,
        [[maybe_unused]] MessageSequenceFilter filter,
        FilterPredicate<std::uint32_t> const& predicate)
    {
        std::scoped_lock lock(m_mutex);
        for (auto const& [id, optDetails] : m_peers) {
            if (!optDetails) {
                continue;
            }

            auto const sequence = optDetails->GetMessageSequenceNumber();

            CallbackIteration result = CallbackIteration::Continue;
            if (predicate(sequence)) {
                result = readFunction(id, optDetails);
            }
            
            if (result == CallbackIteration::Stop) {
                return;
            }
        }
    }

    //------------------------------------------------------------------------------------------------

    void ReadEachPeer(
        ReadMultipleFunction const& readFunction,
        PromotionStateFilter filter)
    {
        std::scoped_lock lock(m_mutex);
        for (auto const& [id, optDetails] : m_peers) {
            CallbackIteration result = CallbackIteration::Continue;
            switch (filter) {
                case PromotionStateFilter::Promoted: {
                    if (optDetails) {
                        result = readFunction(id, optDetails);
                    }
                } break;
                case PromotionStateFilter::Unpromoted: {
                    if (!optDetails) {
                        result = readFunction(id, optDetails);
                    }
                } break;
                default: assert(true); break; // What is this?
            }
            
            if (result == CallbackIteration::Stop) {
                return;
            }
        }
    }

    //------------------------------------------------------------------------------------------------

    void ReadEachPeer(
        ReadMultipleFunction const& readFunction,
        [[maybe_unused]] UpdateTimepointFilter filter,
        FilterPredicate<NodeUtils::Timepoint> const& predicate)
    {
        std::scoped_lock lock(m_mutex);
        for (auto const& [id, optDetails] : m_peers) {
            if (!optDetails) {
                continue;
            }

            auto const timepoint = optDetails->GetUpdateTimepoint();

            CallbackIteration result = CallbackIteration::Continue;
            if (predicate(timepoint)) {
                result = readFunction(id, optDetails);
            }
            
            if (result == CallbackIteration::Stop) {
                return;
            }
        }
    }

    //------------------------------------------------------------------------------------------------


    std::optional<NodeUtils::NodeIdType> Translate(ConnectionIdType const& id)
    {
        std::scoped_lock lock(m_mutex);
        auto const itr = m_peers.find(id);
        if (itr == m_peers.end()) {
            return {};
        }

        OptionalPeerDetails const& optDetails = itr->second;
        if (!optDetails) {
            return {};
        }

        return optDetails->GetNodeId();
    }

    //------------------------------------------------------------------------------------------------

    std::optional<ConnectionIdType> Translate(NodeUtils::NodeIdType id)
    {
        std::scoped_lock lock(m_mutex);
        auto const itr = m_nodeIdLookups.find(id);
        if (itr == m_nodeIdLookups.end()) {
            return {};
        }

        ConnectionIdType const& connectionId = itr->second->first;
        return connectionId;
    }

    //------------------------------------------------------------------------------------------------

    std::uint32_t Size()
    {
        std::scoped_lock lock(m_mutex);
        return m_peers.size();
    }

    //------------------------------------------------------------------------------------------------

private:
    using PeerDetailsMap = typename std::unordered_map<ConnectionIdType, OptionalPeerDetails, ConnectionIdHasher>;
    using PeerDetailsLookup = typename PeerDetailsMap::iterator;
    using NodeIdToPeerLookupMap = std::unordered_map<NodeUtils::NodeIdType, PeerDetailsLookup>;

    mutable std::recursive_mutex m_mutex;
    PeerDetailsMap m_peers; // An unordered map from connection IDs to connection details
    NodeIdToPeerLookupMap m_nodeIdLookups; // An unordered map from node ID to connection details lookup
};

//------------------------------------------------------------------------------------------------