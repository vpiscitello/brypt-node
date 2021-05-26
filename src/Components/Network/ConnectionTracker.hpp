//----------------------------------------------------------------------------------------------------------------------
// File: ConnectionTracker.hpp
// Description: A template class for connection types to store connection information. Allows
// translation between internal connection identifiers (i.e. IPv4 addresses) and Brypt Node IDs.
// Connection connection state information will also be managed by this class (i.e. connection state).
//----------------------------------------------------------------------------------------------------------------------
#pragma once
//----------------------------------------------------------------------------------------------------------------------
#include "ConnectionDetails.hpp"
#include "BryptIdentifier/BryptIdentifier.hpp"
#include "BryptIdentifier/IdentifierTypes.hpp"
#include "BryptIdentifier/ReservedIdentifiers.hpp"
#include "Components/Network/Address.hpp"
#include "Utilities/CallbackIteration.hpp"
#include "Utilities/EnumMaskUtils.hpp"
//----------------------------------------------------------------------------------------------------------------------
#include <boost/multi_index_container.hpp>
#include <boost/multi_index/hashed_index.hpp>
#include <boost/multi_index/mem_fun.hpp>
#include <boost/multi_index/tag.hpp>
//----------------------------------------------------------------------------------------------------------------------
#include <array>
#include <cassert>
#include <functional>
#include <mutex>
#include <optional>
#include <unordered_map>
//----------------------------------------------------------------------------------------------------------------------

template <typename HandleType, typename ExtensionType = void>
class ConnectionEntry
{
public:
    using ExtendedDetails = ConnectionDetails<ExtensionType>;
    using OptionalDetails = std::optional<ExtendedDetails>;

    explicit ConnectionEntry(HandleType const& connection)
        : m_connection(connection)
        , m_optDetails()
    {
    }

    ConnectionEntry(HandleType const& connection, OptionalDetails const& optDetails)
        : m_connection(connection)
        , m_optDetails(optDetails)
    {
    }

    OptionalDetails const& GetDetails() const { return m_optDetails; }
    OptionalDetails& GetUpdatableDetails() { return m_optDetails; }
    HandleType GetHandle() const { return m_connection; }

    Node::Internal::Identifier::Type GetPeerIdentifier() const
    {
        if (!m_optDetails) { return Node::Internal::Identifier::Invalid; }

        auto const spNodeIdentifier = m_optDetails->GetNodeIdentifier();
        if (!spNodeIdentifier) { return Node::Internal::Identifier::Invalid; }

        return spNodeIdentifier->GetInternalValue();
    }
    
    std::string GetUri() const
    {
        if (!m_optDetails) { return {}; }
        return m_optDetails->GetAddress().GetUri();
    }

    void SetConnectionDetails(ExtendedDetails&& details)
    {
        if (m_optDetails && m_optDetails->GetConnectionState() == ConnectionState::Resolving) {
            details.SetAddress(m_optDetails->GetAddress());
        }
        m_optDetails = std::move(details);
    }

private:
    HandleType const m_connection;
    OptionalDetails m_optDetails;
};

//----------------------------------------------------------------------------------------------------------------------

template <typename CopyType>
using CopyFilterPredicate = std::function<bool(CopyType)>;

template <typename ConstReferenceType>
using ConstReferencePredicate = std::function<bool(ConstReferenceType const&)>;

template <typename PredicateType>
using FilterPredicate = std::conditional_t<
    sizeof(PredicateType) <= sizeof(std::uint32_t),
    CopyFilterPredicate<PredicateType>,
    ConstReferencePredicate<PredicateType>>;

//----------------------------------------------------------------------------------------------------------------------

enum class ConnectionStateFilter : std::uint32_t {
    Connected = MaskLevel(std::uint32_t(0)),
    Disconnected = MaskLevel(std::uint32_t(1)),
    Resolving = MaskLevel(std::uint32_t(3)),
    Unknown = MaskLevel(std::uint32_t(4)),
    Invalid = MaskLevel(std::uint32_t(5))
};
ENABLE_ENUM_MASKING(ConnectionStateFilter)

enum class PromotionStateFilter : std::uint32_t { Unpromoted, Promoted };
enum class UpdateTimepointFilter : std::uint32_t { MatchPredicate };

//----------------------------------------------------------------------------------------------------------------------

inline ConnectionStateFilter ConnectionStateToFilter(ConnectionState state)
{
    static const std::unordered_map<ConnectionState, ConnectionStateFilter> equivalents = 
    {
        { ConnectionState::Connected, ConnectionStateFilter::Connected },
        { ConnectionState::Disconnected, ConnectionStateFilter::Disconnected },
        { ConnectionState::Resolving, ConnectionStateFilter::Resolving},
        { ConnectionState::Unknown, ConnectionStateFilter::Unknown }
    };

    if (auto const itr = equivalents.find(state); itr != equivalents.end()) { return itr->second; }

    return ConnectionStateFilter::Invalid;
}

//----------------------------------------------------------------------------------------------------------------------

template <typename HandleType, typename ExtensionType = void>
class ConnectionTracker
{
public:
    using ConnectionEntryType = ConnectionEntry<HandleType, ExtensionType>;
    using ExtendedDetails = ConnectionDetails<ExtensionType>;
    using OptionalDetails = std::optional<ExtendedDetails>;
    
    using ForEachFunction = std::function<CallbackIteration(HandleType const&)>;

    using UpdateOneFunction = std::function<void(ExtendedDetails&)>;
    using UpdateMultipleFunction = std::function<CallbackIteration(HandleType const&, OptionalDetails&)>;
    using UpdateUnpromotedFunction = std::function<ExtendedDetails(Network::RemoteAddress const&)>;

    using ReadOneFunction = std::function<void(ExtendedDetails const&)>;
    using ReadMultipleFunction = std::function<CallbackIteration(HandleType const&, OptionalDetails const&)>;

    struct ConnectionIndex {};
    struct IdentifierIndex {};
    struct UriIndex {};

    ConnectionTracker()
        : m_mutex()
        , m_connections()
    {
    }

    //----------------------------------------------------------------------------------------------------------------------

    void TrackConnection(HandleType const& connection)
    {
        std::scoped_lock lock(m_mutex);
        if (auto const itr = m_connections.find(connection); itr != m_connections.end()) { return; }

        ConnectionEntryType entry(connection);
        m_connections.emplace(entry);
    }

    //----------------------------------------------------------------------------------------------------------------------

    void TrackConnection(HandleType const& connection, Network::RemoteAddress const& address)
    {
        // Return early if the provided remote address is not valid
        if (!address.IsValid()) { return; }

        {
            std::scoped_lock lock(m_mutex);
            if (auto const itr = m_connections.find(connection); itr != m_connections.end()) { return; }

            ConnectionEntryType entry(connection, ExtendedDetails(address));
            m_connections.emplace(entry);
        }
    }

    //----------------------------------------------------------------------------------------------------------------------

    void TrackConnection(HandleType const& connection, ExtendedDetails const& details)
    {
        std::scoped_lock lock(m_mutex);
        if (auto const itr = m_connections.find(connection); itr != m_connections.end()) { return; }

        ConnectionEntryType entry(connection, details);
        m_connections.emplace(entry);
    }

    //----------------------------------------------------------------------------------------------------------------------

    bool PromoteConnection(HandleType const& connection, ExtendedDetails&& details)
    {
        std::scoped_lock lock(m_mutex);
        if(auto const itr = m_connections.find(connection); itr != m_connections.end()) {
            m_connections.modify(itr, [details](ConnectionEntryType& entry) { entry.SetConnectionDetails(details); });
            return true;
        }
        return false;
    }

    //----------------------------------------------------------------------------------------------------------------------

    void UntrackConnection(HandleType connection)
    {
        std::scoped_lock lock(m_mutex);
        m_connections.erase(connection);
    }

    //----------------------------------------------------------------------------------------------------------------------

    void ForEachConnection(ForEachFunction const& callback)
    {
        std::scoped_lock lock(m_mutex);
        for (auto& entry : m_connections) {
            if (callback(entry.GetHandle()) == CallbackIteration::Stop) { return; }
        }
    }

    //----------------------------------------------------------------------------------------------------------------------

    bool UpdateOneConnection(HandleType const& connection, UpdateOneFunction const& callback)
    {
        std::scoped_lock lock(m_mutex);
        if (auto const itr = m_connections.find(connection); itr != m_connections.end()) {
            bool found = false;
            m_connections.modify(itr, [&found, callback] (ConnectionEntryType& entry)
            {
                if (auto& optDetails = entry.GetUpdatableDetails(); optDetails) { callback(*optDetails); found = true; }
            });
            return found;
        }
        return false;
    }

    //----------------------------------------------------------------------------------------------------------------------

    bool UpdateOneConnection(
        HandleType const& connection, UpdateOneFunction const& promoted, UpdateUnpromotedFunction const& unpromoted)
    {
        if (auto const itr = m_connections.find(connection); itr != m_connections.end()) {
            bool found = false;
            m_connections.modify(itr,
                [&found, promoted, unpromoted]( ConnectionEntryType& entry)
                {
                    if (auto& optDetails = entry.GetUpdatableDetails(); optDetails) {
                        if (optDetails->HasAssociatedPeer()) {
                            promoted(*optDetails);
                            found = true;
                        } else {
                            optDetails = unpromoted(optDetails->GetAddress());
                        }
                    } else {
                        optDetails = unpromoted({});
                    }
                });

            return found;
        }

        return true;
    }

    //----------------------------------------------------------------------------------------------------------------------

    void UpdateEachConnection(UpdateMultipleFunction const& callback)
    {
        std::scoped_lock lock(m_mutex);
        for (auto itr = m_connections.begin(); itr != m_connections.end(); ++itr) {
            CallbackIteration result = CallbackIteration::Continue;

            m_connections.modify(itr, [&result, callback](ConnectionEntryType& entry)
            {
                result = callback(entry.GetHandle(), entry.GetUpdatableDetails());
            });

            if (result == CallbackIteration::Stop) { return; }
        }
    }

    //----------------------------------------------------------------------------------------------------------------------

    void UpdateEachConnection(UpdateMultipleFunction const& callback, ConnectionStateFilter filter)
    {
        std::scoped_lock lock(m_mutex);
        for (auto itr = m_connections.begin(); itr != m_connections.end(); ++itr) {
            CallbackIteration result = CallbackIteration::Continue;

            m_connections.modify(itr,
                [&result, callback, filter] (ConnectionEntryType& entry)
                {
                    if (auto& optDetails = entry.GetUpdatableDetails(); optDetails) {
                        auto const state = optDetails->GetConnectionState();
                        auto const filterEquivalent = ConnectionStateToFilter(state);
                        if (FlagIsSet(filterEquivalent, filter)) { result = callback(entry.GetHandle(), optDetails); }
                    }
                });

            if (result == CallbackIteration::Stop) { return; }
        }
    }

    //----------------------------------------------------------------------------------------------------------------------

    void UpdateEachConnection(UpdateMultipleFunction const& callback, PromotionStateFilter filter)
    {
        std::scoped_lock lock(m_mutex);
        for (auto itr = m_connections.begin(); itr != m_connections.end(); ++itr) {
            CallbackIteration result = CallbackIteration::Continue;

            m_connections.modify(itr,
                [&result, callback, filter](ConnectionEntryType& entry)
                {
                    auto& optDetails = entry.GetUpdatableDetails();
                    switch (filter) {
                        case PromotionStateFilter::Promoted: {
                            if (optDetails && optDetails->HasAssociatedPeer()) {
                                result = callback(entry.GetHandle(), optDetails);
                            }
                        } break;
                        case PromotionStateFilter::Unpromoted: {
                            if (!optDetails || (optDetails && !optDetails->HasAssociatedPeer())) {
                                result = callback(entry.GetHandle(), optDetails);
                            }
                        } break;
                        default: assert(false); break; // What is this?
                    }
                });
                
            if (result == CallbackIteration::Stop) { return; }
        }
    }

    //----------------------------------------------------------------------------------------------------------------------

    void UpdateEachConnection(
        UpdateMultipleFunction const& callback,
        [[maybe_unused]] UpdateTimepointFilter filter,
        FilterPredicate<TimeUtils::Timepoint> const& predicate)
    {
        std::scoped_lock lock(m_mutex);
        for (auto itr = m_connections.begin(); itr != m_connections.end(); ++itr) {
            CallbackIteration result = CallbackIteration::Continue;
            m_connections.modify(itr,
                [&result, callback, predicate] (ConnectionEntryType& entry)
                {
                    auto& optDetails = entry.GetUpdatableDetails();
                    if (optDetails) {
                        auto const timepoint = optDetails->GetUpdateTimepoint();
                        if (predicate(timepoint)) { result = callback(entry.GetHandle(), optDetails); }
                    }       
                });
            
            if (result == CallbackIteration::Stop) { return; }
        }
    }

    //----------------------------------------------------------------------------------------------------------------------

    bool ReadOneConnection(HandleType const& connection, ReadOneFunction const& callback)
    {
        std::scoped_lock lock(m_mutex);
        if (auto const itr = m_connections.find(connection); itr != m_connections.end()) {
            if (auto& optDetails = itr->GetDetails(); optDetails) { callback(*optDetails); }
            return true;
        }
        return false;
    }

    //----------------------------------------------------------------------------------------------------------------------

    void ReadEachConnection(ReadMultipleFunction const& callback)
    {
        std::scoped_lock lock(m_mutex);
        for (auto itr = m_connections.begin(); itr != m_connections.end(); ++itr) {
            if (callback(itr->GetHandle(), itr->GetDetails()) == CallbackIteration::Stop) { return; }
        }
    }

    //----------------------------------------------------------------------------------------------------------------------

    void ReadEachConnection(ReadMultipleFunction const& callback, ConnectionStateFilter filter)
    {
        std::scoped_lock lock(m_mutex);
        for (auto itr = m_connections.begin(); itr != m_connections.end(); ++itr) {
            CallbackIteration result = CallbackIteration::Continue;
            
            if (auto const& optDetails = itr->GetDetails(); optDetails) {
                auto const state = optDetails->GetConnectionState();
                auto const filterEquivalent = ConnectionStateToFilter(state);
                if (FlagIsSet(filterEquivalent, filter)) { result = callback(itr->GetHandle(), optDetails); }
            }

            if (result == CallbackIteration::Stop) { return; }
        }
    }

    //----------------------------------------------------------------------------------------------------------------------

    void ReadEachConnection(ReadMultipleFunction const& callback, PromotionStateFilter filter)
    {
        std::scoped_lock lock(m_mutex);
        for (auto itr = m_connections.begin(); itr != m_connections.end(); ++itr) {
            CallbackIteration result = CallbackIteration::Continue;

            auto const& optDetails = itr->GetDetails();
            switch (filter) {
                case PromotionStateFilter::Promoted: {
                    if (optDetails && optDetails->HasAssociatedPeer()) { result = callback(itr->GetHandle(), optDetails); }
                } break;
                case PromotionStateFilter::Unpromoted: {
                    if (!optDetails || (optDetails && !optDetails->HasAssociatedPeer())) {
                        result = callback(itr->GetHandle(), optDetails);
                    }
                } break;
                default: assert(false); break; // What is this?
            }
                
            if (result == CallbackIteration::Stop) { return; }
        }
    }

    //----------------------------------------------------------------------------------------------------------------------

    void ReadEachConnection(
        ReadMultipleFunction const& callback,
        [[maybe_unused]] UpdateTimepointFilter filter,
        FilterPredicate<TimeUtils::Timepoint> const& predicate)
    {
        std::scoped_lock lock(m_mutex);
        for (auto itr = m_connections.begin(); itr != m_connections.end(); ++itr) {
            CallbackIteration result = CallbackIteration::Continue;
            
            if (auto const& optDetails = itr->GetDetails();optDetails) {
                auto const timepoint = optDetails->GetUpdateTimepoint();
                if (predicate(timepoint)) { result = callback(itr->GetHandle(), optDetails); }
            }       
            
            if (result == CallbackIteration::Stop) {
                return;
            }
        }
    }

    //----------------------------------------------------------------------------------------------------------------------

    Node::SharedIdentifier Translate(HandleType const& connection)
    {
        std::scoped_lock lock(m_mutex);
        if (auto const itr = m_connections.find(connection); itr != m_connections.end()) {
            if (auto const& optDetails = itr->GetDetails(); optDetails) { return optDetails->GetNodeIdentifier(); }
        }
        return {};
    }

    //----------------------------------------------------------------------------------------------------------------------

    HandleType Translate(Node::Identifier const& identifier)
    {
        std::scoped_lock lock(m_mutex);
        auto const& index = m_connections.template get<IdentifierIndex>();
        if (auto const itr = index.find(identifier.GetInternalValue()); itr != index.end()) { return itr->GetHandle(); }
        return {};
    }

    //----------------------------------------------------------------------------------------------------------------------

    bool IsUriTracked(std::string_view uri) const
    {
        std::scoped_lock lock(m_mutex);
        auto const& index = m_connections.template get<UriIndex>();
        if (auto const itr = index.find(uri.data()); itr != index.end()) { return true; }
        return false;
    }

    //----------------------------------------------------------------------------------------------------------------------

    std::size_t GetSize() const
    {
        std::scoped_lock lock(m_mutex);
        return m_connections.size();
    }

    //----------------------------------------------------------------------------------------------------------------------

    bool IsEmpty() const
    {
        std::scoped_lock lock(m_mutex);
        return m_connections.empty();
    }

    //----------------------------------------------------------------------------------------------------------------------

    void ResetConnections(UpdateMultipleFunction callback = {})
    {
        if (callback) { UpdateEachConnection(callback); }
        {
            std::scoped_lock lock(m_mutex);
            m_connections.clear();
        }
    }

    //----------------------------------------------------------------------------------------------------------------------

private:
    using ConnectionTrackingMap = boost::multi_index_container<
        ConnectionEntryType,
        boost::multi_index::indexed_by<
            boost::multi_index::hashed_unique<
                boost::multi_index::tag<ConnectionIndex>,
                boost::multi_index::const_mem_fun<
                    ConnectionEntryType,
                    HandleType,
                    &ConnectionEntryType::GetHandle>>,
            boost::multi_index::hashed_non_unique<
                boost::multi_index::tag<IdentifierIndex>,
                boost::multi_index::const_mem_fun<
                    ConnectionEntryType,
                    Node::Internal::Identifier::Type,
                    &ConnectionEntryType::GetPeerIdentifier>>,
            boost::multi_index::hashed_non_unique<
                boost::multi_index::tag<UriIndex>,
                boost::multi_index::const_mem_fun<
                    ConnectionEntryType,
                    std::string,
                    &ConnectionEntryType::GetUri>>>>;

    mutable std::recursive_mutex m_mutex;
    ConnectionTrackingMap m_connections;
};

//----------------------------------------------------------------------------------------------------------------------