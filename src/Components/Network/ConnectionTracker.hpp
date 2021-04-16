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

template <typename ConnectionIdType, typename ExtensionType = void>
class ConnectionEntry
{
public:
    using ExtendedConnectionDetails = ConnectionDetails<ExtensionType>;
    using OptionalConnectionDetails = std::optional<ExtendedConnectionDetails>;

    explicit ConnectionEntry(ConnectionIdType const& connection)
        : m_connection(connection)
        , m_optConnectionDetails()
    {
    }

    ConnectionEntry(ConnectionIdType const& connection, OptionalConnectionDetails const& optDetails)
        : m_connection(connection)
        , m_optConnectionDetails(optDetails)
    {
    }

    OptionalConnectionDetails const& GetConnectionDetails() const
    {
        return m_optConnectionDetails;
    }

    ConnectionIdType GetConnectionIdentifier() const
    {
        return m_connection;
    }

    Node::Internal::Identifier::Type GetPeerIdentifier() const
    {
        if (!m_optConnectionDetails) { return Node::Internal::Identifier::Invalid; }

        auto const spNodeIdentifier = m_optConnectionDetails->GetNodeIdentifier();
        if (!spNodeIdentifier) { return Node::Internal::Identifier::Invalid; }

        return spNodeIdentifier->GetInternalValue();
    }
    
    std::string GetUri() const
    {
        if (!m_optConnectionDetails) { return {}; }

        return m_optConnectionDetails->GetAddress().GetUri();
    }

    OptionalConnectionDetails& GetUpdatableConnectionDetails()
    {
        return m_optConnectionDetails;
    }

    void SetConnectionDetails(ExtendedConnectionDetails&& details)
    {
        if (m_optConnectionDetails &&
            m_optConnectionDetails->GetConnectionState() == ConnectionState::Resolving) {
            details.SetAddress(m_optConnectionDetails->GetAddress());
        }

        m_optConnectionDetails = std::move(details);
    }

private:
    ConnectionIdType const m_connection;
    OptionalConnectionDetails m_optConnectionDetails;
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

enum class ConnectionStateFilter : std::uint8_t {
    Connected = MaskLevel(std::uint8_t(0)),
    Disconnected = MaskLevel(std::uint8_t(1)),
    Resolving = MaskLevel(std::uint8_t(3)),
    Unknown = MaskLevel(std::uint8_t(4)),
    Invalid = MaskLevel(std::uint8_t(5))
};

ENABLE_ENUM_MASKING(ConnectionStateFilter)

//----------------------------------------------------------------------------------------------------------------------

enum class MessageSequenceFilter : std::uint8_t {
    MatchPredicate
};

//----------------------------------------------------------------------------------------------------------------------

enum class PromotionStateFilter : std::uint8_t {
    Unpromoted,
    Promoted
};

//----------------------------------------------------------------------------------------------------------------------

enum class UpdateTimepointFilter : std::uint8_t {
    MatchPredicate
};

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

    if (auto const itr = equivalents.find(state); itr != equivalents.end()) {
        return itr->second;
    }

    return ConnectionStateFilter::Invalid;
}

//----------------------------------------------------------------------------------------------------------------------

template <typename ConnectionIdType, typename ExtensionType = void>
class ConnectionTracker
{
public:
    using ConnectionEntryType = ConnectionEntry<ConnectionIdType, ExtensionType>;
    using ExtendedConnectionDetails = ConnectionDetails<ExtensionType>;
    using OptionalConnectionDetails = std::optional<ExtendedConnectionDetails>;
    
    using ForEachFunction = std::function<CallbackIteration(ConnectionIdType const&)>;

    using UpdateOneFunction = std::function<void(ExtendedConnectionDetails&)>;
    using UpdateMultipleFunction = std::function<
        CallbackIteration(ConnectionIdType const&, OptionalConnectionDetails&)>;
    using UpdateUnpromotedConnectionFunction = std::function<
        ExtendedConnectionDetails(Network::RemoteAddress const&)>;

    using ReadOneFunction = std::function<void(ExtendedConnectionDetails const&)>;
    using ReadMultipleFunction = std::function<
        CallbackIteration(ConnectionIdType const&, OptionalConnectionDetails const&)>;

    struct ConnectionIndex {};
    struct IdentifierIndex {};
    struct UriIndex {};

    ConnectionTracker()
        : m_mutex()
        , m_connections()
    {
    }

    //----------------------------------------------------------------------------------------------------------------------

    void TrackConnection(ConnectionIdType const& connection)
    {
        std::scoped_lock lock(m_mutex);
        if (auto const itr = m_connections.find(connection); itr != m_connections.end()) { return; }

        ConnectionEntryType entry(connection);
        m_connections.emplace(entry);
    }

    //----------------------------------------------------------------------------------------------------------------------

    void TrackConnection(ConnectionIdType const& connection, Network::RemoteAddress const& address)
    {
        // Return early if the provided remote address is not valid
        if (!address.IsValid()) { return; }

        {
            std::scoped_lock lock(m_mutex);
            if (auto const itr = m_connections.find(connection); itr != m_connections.end()) {
                return;
            }

            ConnectionEntryType entry(connection, ExtendedConnectionDetails(address));
            m_connections.emplace(entry);
        }
    }

    //----------------------------------------------------------------------------------------------------------------------

    void TrackConnection(
        ConnectionIdType const& connection, ExtendedConnectionDetails const& details)
    {
        std::scoped_lock lock(m_mutex);
        if (auto const itr = m_connections.find(connection); itr != m_connections.end()) {
            return;
        }

        ConnectionEntryType entry(connection, details);
        m_connections.emplace(entry);
    }

    //----------------------------------------------------------------------------------------------------------------------

    bool PromoteConnection(ConnectionIdType const& connection, ExtendedConnectionDetails&& details)
    {
        std::scoped_lock lock(m_mutex);
        if(auto const itr = m_connections.find(connection); itr != m_connections.end()) {
            m_connections.modify(itr, [details](ConnectionEntryType& entry) {
                entry.SetConnectionDetails(details);
            });
            return true;
        }

        return false;
    }

    //----------------------------------------------------------------------------------------------------------------------

    void UntrackConnection(ConnectionIdType connection)
    {
        std::scoped_lock lock(m_mutex);
        m_connections.erase(connection);
    }

    //----------------------------------------------------------------------------------------------------------------------

    void ForEachConnection(ForEachFunction const& forEachFunction)
    {
        std::scoped_lock lock(m_mutex);
        for (auto& entry : m_connections) {
            auto const result = forEachFunction(entry.GetConnectionIdentifier());
            if (result == CallbackIteration::Stop) {
                return;
            }
        }
    }

    //----------------------------------------------------------------------------------------------------------------------

    bool UpdateOneConnection(
        ConnectionIdType const& connection, UpdateOneFunction const& updateFunction)
    {
        std::scoped_lock lock(m_mutex);
        if (auto const itr = m_connections.find(connection); itr != m_connections.end()) {
            bool found = false;
            m_connections.modify(itr, [&found, updateFunction](ConnectionEntryType& entry)
            {
                auto& optConnectionDetails = entry.GetUpdatableConnectionDetails();
                if (optConnectionDetails) {
                    updateFunction(*optConnectionDetails);
                    found = true;
                }
            });

            return found;
        }

        return false;
    }

    //----------------------------------------------------------------------------------------------------------------------

    bool UpdateOneConnection(
        ConnectionIdType const& connection,
        UpdateOneFunction const& promotedConnectionFunction,
        UpdateUnpromotedConnectionFunction const& unpromotedConnectionFunction)
    {
        if (auto const itr = m_connections.find(connection); itr != m_connections.end()) {
            bool found = false;
            m_connections.modify(itr,
                [&found, promotedConnectionFunction, unpromotedConnectionFunction](
                    ConnectionEntryType& entry)
                {
                    auto& optConnectionDetails = entry.GetUpdatableConnectionDetails();
                    if (optConnectionDetails) {
                        if (optConnectionDetails->HasAssociatedPeer()) {
                            promotedConnectionFunction(*optConnectionDetails);
                            found = true;
                        } else {
                            optConnectionDetails = unpromotedConnectionFunction(
                                optConnectionDetails->GetAddress());
                        }
                    } else {
                        optConnectionDetails = unpromotedConnectionFunction({});
                    }
                });

            return found;
        }

        return true;
    }

    //----------------------------------------------------------------------------------------------------------------------

    void UpdateEachConnection(UpdateMultipleFunction const& updateFunction)
    {
        std::scoped_lock lock(m_mutex);
        for (auto itr = m_connections.begin(); itr != m_connections.end(); ++itr) {
            CallbackIteration result = CallbackIteration::Continue;

            m_connections.modify(itr, [&result, updateFunction](ConnectionEntryType& entry)
            {
                result = updateFunction(
                    entry.GetConnectionIdentifier(), entry.GetUpdatableConnectionDetails());
            });

            if (result == CallbackIteration::Stop) {
                return;
            }
        }
    }

    //----------------------------------------------------------------------------------------------------------------------

    void UpdateEachConnection(
        UpdateMultipleFunction const& updateFunction,
        ConnectionStateFilter filter)
    {
        std::scoped_lock lock(m_mutex);
        for (auto itr = m_connections.begin(); itr != m_connections.end(); ++itr) {
            CallbackIteration result = CallbackIteration::Continue;

            m_connections.modify(itr,
                [&result, updateFunction, filter](ConnectionEntryType& entry)
                {
                    auto& optConnectionDetails = entry.GetUpdatableConnectionDetails();
                    if (optConnectionDetails) {
                        auto const state = optConnectionDetails->GetConnectionState();
                        auto const filterEquivalent = ConnectionStateToFilter(state);

                        if (FlagIsSet(filterEquivalent, filter)) {
                            result = updateFunction(
                                entry.GetConnectionIdentifier(), optConnectionDetails);
                        }
                    }
                });

            if (result == CallbackIteration::Stop) {
                return;
            }
        }
    }

    //----------------------------------------------------------------------------------------------------------------------

    void UpdateEachConnection(
        UpdateMultipleFunction const& updateFunction,
        [[maybe_unused]] MessageSequenceFilter filter,
        FilterPredicate<std::uint32_t> const& predicate)
    {
        std::scoped_lock lock(m_mutex);
        for (auto itr = m_connections.begin(); itr != m_connections.end(); ++itr) {
            CallbackIteration result = CallbackIteration::Continue;

            m_connections.modify(itr,
                [&result, updateFunction, predicate](ConnectionEntryType& entry)
                {
                    auto& optConnectionDetails = entry.GetUpdatableConnectionDetails();
                    if (optConnectionDetails) {
                        auto const sequence = optConnectionDetails->GetMessageSequenceNumber();
                        if (predicate(sequence)) {
                            result = updateFunction(
                                entry.GetConnectionIdentifier(), optConnectionDetails);
                        }
                    }
                });

            if (result == CallbackIteration::Stop) {
                return;
            }
        }
    }

    //----------------------------------------------------------------------------------------------------------------------

    void UpdateEachConnection(
        UpdateMultipleFunction const& updateFunction,
        PromotionStateFilter filter)
    {
        std::scoped_lock lock(m_mutex);
        for (auto itr = m_connections.begin(); itr != m_connections.end(); ++itr) {
            CallbackIteration result = CallbackIteration::Continue;

            m_connections.modify(itr,
                [&result, updateFunction, filter](ConnectionEntryType& entry)
                {
                    auto& optConnectionDetails = entry.GetUpdatableConnectionDetails();
                    switch (filter) {
                        case PromotionStateFilter::Promoted: {
                            if (optConnectionDetails && optConnectionDetails->HasAssociatedPeer()) {
                                result = updateFunction(
                                    entry.GetConnectionIdentifier(), optConnectionDetails);
                            }
                        } break;
                        case PromotionStateFilter::Unpromoted: {
                            if (!optConnectionDetails ||
                                (optConnectionDetails && !optConnectionDetails->HasAssociatedPeer())) {
                                result = updateFunction(
                                    entry.GetConnectionIdentifier(), optConnectionDetails);
                            }
                        } break;
                        default: assert(false); break; // What is this?
                    }
                });
                
            if (result == CallbackIteration::Stop) {
                return;
            }
        }
    }

    //----------------------------------------------------------------------------------------------------------------------

    void UpdateEachConnection(
        UpdateMultipleFunction const& updateFunction,
        [[maybe_unused]] UpdateTimepointFilter filter,
        FilterPredicate<TimeUtils::Timepoint> const& predicate)
    {
        std::scoped_lock lock(m_mutex);
        for (auto itr = m_connections.begin(); itr != m_connections.end(); ++itr) {
            CallbackIteration result = CallbackIteration::Continue;
            m_connections.modify(itr,
                [&result, updateFunction, predicate](ConnectionEntryType& entry)
                {
                    auto& optConnectionDetails = entry.GetUpdatableConnectionDetails();
                    if (optConnectionDetails) {
                        auto const timepoint = optConnectionDetails->GetUpdateTimepoint();
                        if (predicate(timepoint)) {
                            result = updateFunction(
                                entry.GetConnectionIdentifier(), optConnectionDetails);
                        }
                    }       
                });
            
            if (result == CallbackIteration::Stop) {
                return;
            }
        }
    }

    //----------------------------------------------------------------------------------------------------------------------

    bool ReadOneConnection(ConnectionIdType const& connection, ReadOneFunction const& readFunction)
    {
        std::scoped_lock lock(m_mutex);
        if (auto const itr = m_connections.find(connection); itr != m_connections.end()) {
            auto& optConnectionDetails = itr->GetConnectionDetails();
            if (optConnectionDetails) {
                readFunction(*optConnectionDetails);
            }

            return true;
        }

        return false;
    }

    //----------------------------------------------------------------------------------------------------------------------

    void ReadEachConnection(ReadMultipleFunction const& readFunction)
    {
        std::scoped_lock lock(m_mutex);
        for (auto itr = m_connections.begin(); itr != m_connections.end(); ++itr) {
            CallbackIteration result = readFunction(
                    itr->GetConnectionIdentifier(), itr->GetConnectionDetails());
            if (result == CallbackIteration::Stop) {
                return;
            }
        }
    }

    //----------------------------------------------------------------------------------------------------------------------

    void ReadEachConnection(
        ReadMultipleFunction const& readFunction,
        ConnectionStateFilter filter)
    {
        std::scoped_lock lock(m_mutex);
        for (auto itr = m_connections.begin(); itr != m_connections.end(); ++itr) {
            CallbackIteration result = CallbackIteration::Continue;

            auto const& optConnectionDetails = itr->GetConnectionDetails();
            if (optConnectionDetails) {
                auto const state = optConnectionDetails->GetConnectionState();
                auto const filterEquivalent = ConnectionStateToFilter(state);

                if (FlagIsSet(filterEquivalent, filter)) {
                    result = readFunction(
                        itr->GetConnectionIdentifier(), optConnectionDetails);
                }
            }

            if (result == CallbackIteration::Stop) {
                return;
            }
        }
    }

    //----------------------------------------------------------------------------------------------------------------------

    void ReadEachConnection(
        ReadMultipleFunction const& readFunction,
        [[maybe_unused]] MessageSequenceFilter filter,
        FilterPredicate<std::uint32_t> const& predicate)
    {
        std::scoped_lock lock(m_mutex);
        for (auto itr = m_connections.begin(); itr != m_connections.end(); ++itr) {
            CallbackIteration result = CallbackIteration::Continue;

            auto const& optConnectionDetails = itr->GetConnectionDetails();
            if (optConnectionDetails) {
                auto const sequence = optConnectionDetails->GetMessageSequenceNumber();
                if (predicate(sequence)) {
                    result = readFunction(
                        itr->GetConnectionIdentifier(), optConnectionDetails);
                }
            }

            if (result == CallbackIteration::Stop) {
                return;
            }
        }
    }

    //----------------------------------------------------------------------------------------------------------------------

    void ReadEachConnection(
        ReadMultipleFunction const& readFunction,
        PromotionStateFilter filter)
    {
        std::scoped_lock lock(m_mutex);
        for (auto itr = m_connections.begin(); itr != m_connections.end(); ++itr) {
            CallbackIteration result = CallbackIteration::Continue;

            auto const& optConnectionDetails = itr->GetConnectionDetails();
            switch (filter) {
                case PromotionStateFilter::Promoted: {
                    if (optConnectionDetails && optConnectionDetails->HasAssociatedPeer()) {
                        result = readFunction(
                            itr->GetConnectionIdentifier(), optConnectionDetails);
                    }
                } break;
                case PromotionStateFilter::Unpromoted: {
                    if (!optConnectionDetails ||
                        (optConnectionDetails && !optConnectionDetails->HasAssociatedPeer())) {
                        result = readFunction(
                            itr->GetConnectionIdentifier(), optConnectionDetails);
                    }
                } break;
                default: assert(false); break; // What is this?
            }
                
            if (result == CallbackIteration::Stop) {
                return;
            }
        }
    }

    //----------------------------------------------------------------------------------------------------------------------

    void ReadEachConnection(
        ReadMultipleFunction const& readFunction,
        [[maybe_unused]] UpdateTimepointFilter filter,
        FilterPredicate<TimeUtils::Timepoint> const& predicate)
    {
        std::scoped_lock lock(m_mutex);
        for (auto itr = m_connections.begin(); itr != m_connections.end(); ++itr) {
            CallbackIteration result = CallbackIteration::Continue;

            auto const& optConnectionDetails = itr->GetConnectionDetails();
            if (optConnectionDetails) {
                auto const timepoint = optConnectionDetails->GetUpdateTimepoint();
                if (predicate(timepoint)) {
                    result = readFunction(
                        itr->GetConnectionIdentifier(), optConnectionDetails);
                }
            }       
            
            if (result == CallbackIteration::Stop) {
                return;
            }
        }
    }

    //----------------------------------------------------------------------------------------------------------------------


    Node::SharedIdentifier Translate(ConnectionIdType const& connection)
    {
        std::scoped_lock lock(m_mutex);
        if (auto const itr = m_connections.find(connection); itr != m_connections.end()) {
            auto const& optConnectionDetails = itr->GetConnectionDetails();
            if (optConnectionDetails) {
                return optConnectionDetails->GetNodeIdentifier();
            }
        }
        return {};
    }

    //----------------------------------------------------------------------------------------------------------------------

    ConnectionIdType Translate(Node::Identifier const& identifier)
    {
        std::scoped_lock lock(m_mutex);
        auto const& index = m_connections.template get<IdentifierIndex>();
        if (auto const itr = index.find(identifier.GetInternalValue()); itr != index.end()) {
            return itr->GetConnectionIdentifier();
        }
        return {};
    }

    //----------------------------------------------------------------------------------------------------------------------

    bool IsUriTracked(std::string_view uri) const
    {
        std::scoped_lock lock(m_mutex);
        auto const& index = m_connections.template get<UriIndex>();
        if (auto const itr = index.find(uri.data()); itr != index.end()) {
            return true;
        }
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
                    ConnectionIdType,
                    &ConnectionEntryType::GetConnectionIdentifier>>,
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