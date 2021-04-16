//----------------------------------------------------------------------------------------------------------------------
// File: ConnectionDetails.hpp
// Description: 
//----------------------------------------------------------------------------------------------------------------------
#pragma once
//----------------------------------------------------------------------------------------------------------------------
#include "ConnectionState.hpp"
#include "Address.hpp"
#include "BryptIdentifier/BryptIdentifier.hpp"
#include "Components/Peer/Proxy.hpp"
#include "Utilities/TimeUtils.hpp"
//----------------------------------------------------------------------------------------------------------------------
#include <functional>
//----------------------------------------------------------------------------------------------------------------------

class ConnectionDetailsBase
{
public:
    explicit ConnectionDetailsBase(Network::RemoteAddress const& address)
        : m_address(address)
        , m_updateTimepoint()
        , m_sequenceNumber(0)
        , m_connectionState(ConnectionState::Resolving)
        , m_spPeer()
    {
    }

    explicit ConnectionDetailsBase(std::shared_ptr<Peer::Proxy> const& spPeerProxy)
        : m_address()
        , m_updateTimepoint()
        , m_sequenceNumber(0)
        , m_connectionState(ConnectionState::Resolving)
        , m_spPeer(spPeerProxy)
    {
    }

    Network::RemoteAddress GetAddress() const { return m_address; }
    TimeUtils::Timepoint GetUpdateTimepoint() const { return m_updateTimepoint; }
    std::uint32_t GetMessageSequenceNumber() const { return m_sequenceNumber; }
    ConnectionState GetConnectionState() const { return m_connectionState; }
    std::shared_ptr<Peer::Proxy> GetPeerProxy() const { return m_spPeer; }
    Node::SharedIdentifier GetNodeIdentifier() const
    {
        if (!m_spPeer) {
            return {};
        }
        return m_spPeer->GetNodeIdentifier();
    }

    void SetAddress(Network::RemoteAddress const& address)
    {
        m_address = address;
    }

    void SetUpdatedTimepoint(TimeUtils::Timepoint const& timepoint)
    {
        m_updateTimepoint = timepoint;
    }

    void SetMessageSequenceNumber(std::uint32_t sequenceNumber)
    {
        m_sequenceNumber = sequenceNumber;
    }

    void IncrementMessageSequence()
    {
        ++m_sequenceNumber;
    }

    void SetConnectionState(ConnectionState state)
    {
        m_connectionState = state;
        Updated();
    }

    void SetPeerProxy(std::shared_ptr<Peer::Proxy> const& spPeerProxy)
    {
        m_spPeer = spPeerProxy;
    }

    void Updated()
    {
        m_updateTimepoint = TimeUtils::GetSystemTimepoint();
    }

    bool HasAssociatedPeer() const 
    {
        if (!m_spPeer) {
            return false;
        }

        return true;
    }

protected: 
    Network::RemoteAddress m_address;
    TimeUtils::Timepoint m_updateTimepoint;
    std::uint32_t m_sequenceNumber;
    ConnectionState m_connectionState;
    
    std::shared_ptr<Peer::Proxy> m_spPeer;

};

//----------------------------------------------------------------------------------------------------------------------

template <typename ExtensionType = void, typename Enabled = void>
class ConnectionDetails;

//----------------------------------------------------------------------------------------------------------------------

template <typename ExtensionType>
class ConnectionDetails<ExtensionType, std::enable_if_t<
    std::is_same_v<ExtensionType, void>>> : public ConnectionDetailsBase
{
public:
    using ConnectionDetailsBase::ConnectionDetailsBase;

    ConnectionDetails(ConnectionDetails const& other) = default;
    ConnectionDetails(ConnectionDetails&& other) = default;

    ConnectionDetails& operator=(ConnectionDetails const& other)
    {
        if (other.m_address.IsValid()) { m_address = other.m_address;}
        m_updateTimepoint = other.m_updateTimepoint;
        m_sequenceNumber = other.m_sequenceNumber;
        m_connectionState = other.m_connectionState;
        m_spPeer = other.m_spPeer;
        return *this;
    }

};

//----------------------------------------------------------------------------------------------------------------------

template <typename ExtensionType>
class ConnectionDetails<ExtensionType, std::enable_if_t<
    std::is_class_v<ExtensionType>>> : public ConnectionDetailsBase
{
public:
    using ConnectionDetailsBase::ConnectionDetailsBase;

    using ReadExtensionFunction = std::function<void(ExtensionType const&)>;
    using UpdateExtensionFunction = std::function<void(ExtensionType&)>;

    ConnectionDetails(
        Node::SharedIdentifier const& spNodeIdentifier,
        ExtensionType const& extension)
        : ConnectionDetailsBase(spNodeIdentifier)
        , m_extension(extension)
    {
    }

    ConnectionDetails(
        Network::RemoteAddress const& address,
        ExtensionType const& extension)
        : ConnectionDetailsBase(address)
        , m_extension(extension)
    {
    }

    ConnectionDetails(ConnectionDetails const& other) = default;
    ConnectionDetails(ConnectionDetails&& other) = default;

    ConnectionDetails& operator=(ConnectionDetails const& other)
    {
        if (other.m_address.IsValid()) { m_address = other.m_address; }
        m_updateTimepoint = other.m_updateTimepoint;
        m_sequenceNumber = other.m_sequenceNumber;
        m_connectionState = other.m_connectionState;
        m_spPeer = other.spPeerProxy;
        m_extension = other.m_extension;
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
    // Each connection must define an extension class/struct that contains information 
    // it cares to track about each node. 
    ExtensionType m_extension;

};

//----------------------------------------------------------------------------------------------------------------------