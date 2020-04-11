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

enum class ConnectionState : std::uint8_t {
    Connected,
    Disconnected,
    Flagged,
    Resolving,
    Unknown
};

//------------------------------------------------------------------------------------------------

enum class MessagingPhase : std::uint8_t {
    Request,
    Response
};

//------------------------------------------------------------------------------------------------

class CPeerDetailsBase
{
public:
    CPeerDetailsBase(
        NodeUtils::NodeIdType id,
        ConnectionState connectionState,
        MessagingPhase messagingPhase)
        : m_id(id)
        , m_updateTimepoint()
        , m_sequenceNumber(0)
        , m_connectionState(connectionState)
        , m_messagingPhase(messagingPhase)
    {
    }

    CPeerDetailsBase(
        NodeUtils::NodeIdType id,
        NodeUtils::Timepoint const& timepoint,
        std::uint32_t sequenceNumber,
        ConnectionState connectionState,
        MessagingPhase messagingPhase)
        : m_id(id)
        , m_updateTimepoint(timepoint)
        , m_sequenceNumber(sequenceNumber)
        , m_connectionState(connectionState)
        , m_messagingPhase(messagingPhase)
    {
    }

    NodeUtils::NodeIdType GetNodeId() const { return m_id; }
    NodeUtils::Timepoint GetUpdateTimepoint() const { return m_updateTimepoint; }
    std::uint32_t GetMessageSequenceNumber() const { return m_sequenceNumber; }
    ConnectionState GetConnectionState() const { return m_connectionState; }
    MessagingPhase GetMessagingPhase() const { return m_messagingPhase; }

    void Updated() { m_updateTimepoint = NodeUtils::GetSystemTimepoint(); };

    void IncrementMessageSequence() {
        ++m_sequenceNumber;
    }

    void SetConnectionState(ConnectionState state) {
        m_connectionState = state;
        Updated();
    }

    void SetMessagingPhase(MessagingPhase phase)
    {
        m_messagingPhase = phase;
        Updated();
    }

protected: 
    // Currently it is expected each connection type maintains information about the node's Brypt ID,
    // the timepoint the connection was last updated, and the message sequence number.
    NodeUtils::NodeIdType m_id;
    NodeUtils::Timepoint m_updateTimepoint;
    std::uint32_t m_sequenceNumber;
    ConnectionState m_connectionState;
    MessagingPhase m_messagingPhase;

};

//------------------------------------------------------------------------------------------------

template <typename ExtensionType = void, typename Enabled = void>
class CPeerDetails;

//------------------------------------------------------------------------------------------------

template <typename ExtensionType>
class CPeerDetails<ExtensionType, std::enable_if_t<std::is_same_v<ExtensionType, void>>> : public CPeerDetailsBase
{
public:
    using CPeerDetailsBase::CPeerDetailsBase;

    CPeerDetails(CPeerDetails const& other) = default;
    CPeerDetails(CPeerDetails&& other) = default;

    CPeerDetails& operator=(CPeerDetails const& other)
    {
        m_id = other.GetNodeId();
        m_updateTimepoint = other.GetUpdateTimepoint();
        m_sequenceNumber = other.GetMessageSequenceNumber();
        m_connectionState = other.GetConnectionState();
        return *this;
    }

};

//------------------------------------------------------------------------------------------------

template <typename ExtensionType>
class CPeerDetails<ExtensionType, std::enable_if_t<std::is_class_v<ExtensionType>>> : public CPeerDetailsBase
{
public:
    using CPeerDetailsBase::CPeerDetailsBase;

    using ReadExtensionFunction = std::function<void(ExtensionType const&)>;
    using UpdateExtensionFunction = std::function<void(ExtensionType&)>;

    CPeerDetails(
        NodeUtils::NodeIdType id,
        ConnectionState connectionState,
        MessagingPhase messagePhase,
        ExtensionType const& extension)
        : CPeerDetailsBase(id, connectionState, messagePhase)
        , m_extension(extension)
    {
    }

    CPeerDetails(CPeerDetails const& other) = default;
    CPeerDetails(CPeerDetails&& other) = default;

    CPeerDetails& operator=(CPeerDetails const& other)
    {
        m_id = other.GetNodeId();
        m_updateTimepoint = other.GetUpdateTimepoint();
        m_sequenceNumber = other.GetMessageSequenceNumber();
        m_connectionState = other.GetConnectionState();
        m_extension = other.GetExtension();
        return *this;
    }

    ExtensionType GetExtension() const { return m_extension; }

    void ReadExtension(ReadExtensionFunction const& readFunction) const
    {
        readFunction(m_extension);
    }
    
    void UpdateExtension(UpdateExtensionFunction const& updateFunction)
    {
        updateFunction(m_extension);
    }

private:
    // Each connection must define an extension class/struct that contains information it cares to track
    // about each node. 
    ExtensionType m_extension;

};

//------------------------------------------------------------------------------------------------