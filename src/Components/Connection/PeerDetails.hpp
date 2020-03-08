//------------------------------------------------------------------------------------------------
// File: PeerDetailsMap.hpp
// Description: Classes used to track information about a connected peer. There are two instances
// of the PeerDetail struct, most notably one may be instantiated with an extension struct/class
// to track additionaly data pertaint to the caller. 
//------------------------------------------------------------------------------------------------
#pragma once
//------------------------------------------------------------------------------------------------
#include "../../Utilities/NodeUtils.hpp"
//------------------------------------------------------------------------------------------------
#include <functional>
//------------------------------------------------------------------------------------------------

enum class ConnectionState {
    Negotiating,
    Authenticating,
    Connected,
    Disconnected,
    Flagged,
    Unknown
};

//------------------------------------------------------------------------------------------------

template <typename ExtensionType = void>
class CPeerDetails
{
public:
    CPeerDetails(NodeUtils::NodeIdType id)
        : m_id(id)
        , m_updateTimePoint()
        , m_sequenceNumber(0)
        , m_connectionState(ConnectionState::Unknown)
        , m_extension()
    {
        static_assert(std::is_class<ExtensionType>::value, "The provided extension type must be a class or struct!");
    }

    using ReadExtensionFunction = std::function<void(ExtensionType const&)>;
    using UpdateExtensionFunction = std::function<void(ExtensionType&)>;

    CPeerDetails(
        NodeUtils::NodeIdType id,
        ExtensionType const& extension)
        : m_id(id)
        , m_updateTimePoint()
        , m_sequenceNumber(0)
        , m_connectionState(ConnectionState::Unknown)
        , m_extension(extension)
    {
        static_assert(std::is_class<ExtensionType>::value, "The provided extension type must be a class or struct!");
    }

    NodeUtils::NodeIdType GetNodeId() const { return m_id; }
    NodeUtils::TimePoint GetUpdateTimePoint() const { return m_updateTimePoint; }
    std::uint32_t GetMessageSequenceNumber() const { return m_sequenceNumber; }
    ConnectionState GetConnectionState() const { return m_connectionState; }

    void Updated() { m_updateTimePoint = NodeUtils::GetSystemTimePoint(); };
    void IncrementMessageSequence() {
        ++m_sequenceNumber;
        Updated();
    }
    void SetConnectionState(ConnectionState state) {
        m_connectionState = state;
        Updated();
    }

    void ReadExtension(ReadExtensionFunction const& readFunction) const
    {
        readFunction(m_extension);
    }
    void UpdateExtension(UpdateExtensionFunction const& updateFunction) const
    {
        updateFunction(m_extension);
    }

private:
    NodeUtils::NodeIdType const m_id;
    NodeUtils::TimePoint m_updateTimePoint;
    std::uint32_t m_sequenceNumber;
    ConnectionState m_connectionState;

    // Each connection must define an extension class/struct that contains information it cares to track
    // about each node. 
    ExtensionType m_extension;
};

//------------------------------------------------------------------------------------------------

template <>
class CPeerDetails<void>
{
public:
    explicit CPeerDetails(NodeUtils::NodeIdType id)
        : m_id(id)
        , m_updateTimePoint()
        , m_sequenceNumber(0)
        , m_connectionState(ConnectionState::Unknown)
    {
    }

    NodeUtils::NodeIdType GetNodeId() const { return m_id; }
    NodeUtils::TimePoint GetUpdateTimePoint() const { return m_updateTimePoint; }
    std::uint32_t GetMessageSequenceNumber() const { return m_sequenceNumber; }
    ConnectionState GetConnectionState() const { return m_connectionState; }

    void Updated() { m_updateTimePoint = NodeUtils::GetSystemTimePoint(); };
    void IncrementMessageSequence() {
        ++m_sequenceNumber;
        Updated();
    }
    void SetConnectionState(ConnectionState state) {
        m_connectionState = state;
        Updated();
    }

private: 
    // Currently it is expected each connection type maintains information about the node's Brypt ID,
    // the timepoint the connection was last updated, and the message sequence number.
    NodeUtils::NodeIdType const m_id;
    NodeUtils::TimePoint m_updateTimePoint;
    std::uint32_t m_sequenceNumber;
    ConnectionState m_connectionState;
};

//------------------------------------------------------------------------------------------------

