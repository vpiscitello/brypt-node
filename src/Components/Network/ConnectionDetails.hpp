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
        , m_updated()
        , m_state(Network::Connection::State::Resolving)
        , m_spPeerProxy()
    {
    }

    explicit ConnectionDetailsBase(std::shared_ptr<Peer::Proxy> const& spPeerProxy)
        : m_address()
        , m_updated()
        , m_state(Network::Connection::State::Resolving)
        , m_spPeerProxy(spPeerProxy)
    {
    }

    Network::RemoteAddress GetAddress() const { return m_address; }
    TimeUtils::Timepoint GetUpdateTimepoint() const { return m_updated; }
    Network::Connection::State GetConnectionState() const { return m_state; }
    std::shared_ptr<Peer::Proxy> GetPeerProxy() const { return m_spPeerProxy; }
    Node::SharedIdentifier GetNodeIdentifier() const
    {
        if (!m_spPeerProxy) { return {}; }
        return m_spPeerProxy->GetIdentifier();
    }

    void SetAddress(Network::RemoteAddress const& address){ m_address = address; }
    void SetUpdatedTimepoint(TimeUtils::Timepoint const& timepoint) { m_updated = timepoint; }
    void SetConnectionState(Network::Connection::State state) { m_state = state; Updated(); }
    void SetPeerProxy(std::shared_ptr<Peer::Proxy> const& spPeerProxy) { m_spPeerProxy = spPeerProxy; }
    void Updated() { m_updated = TimeUtils::GetSystemTimepoint(); }
    bool HasAssociatedPeer() const { return m_spPeerProxy != nullptr; }

protected: 
    Network::RemoteAddress m_address;
    TimeUtils::Timepoint m_updated;
    Network::Connection::State m_state;
    std::shared_ptr<Peer::Proxy> m_spPeerProxy;
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
        m_updated = other.m_updated;
        m_state = other.m_state;
        m_spPeerProxy = other.m_spPeerProxy;
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

    ConnectionDetails(Network::RemoteAddress const& address, ExtensionType const& extension)
        : ConnectionDetailsBase(address)
        , m_extension(extension)
    {
    }

    ConnectionDetails(Node::SharedIdentifier const& spNodeIdentifier, ExtensionType const& extension)
        : ConnectionDetailsBase(spNodeIdentifier)
        , m_extension(extension)
    {
    }

    ConnectionDetails(ConnectionDetails const& other) = default;
    ConnectionDetails(ConnectionDetails&& other) = default;

    ConnectionDetails& operator=(ConnectionDetails const& other)
    {
        if (other.m_address.IsValid()) { m_address = other.m_address; }
        m_updated = other.m_updated;
        m_state = other.m_state;
        m_spPeerProxy = other.spPeerProxy;
        m_extension = other.m_extension;
        return *this;
    }

    ExtensionType GetExtension() const { return m_extension; }
    void ReadExtension(ReadExtensionFunction const& readFunction) const { readFunction(m_extension); }
    void UpdateExtension(UpdateExtensionFunction const& updateFunction) { updateFunction(m_extension); }

private:
    ExtensionType m_extension;
};

//----------------------------------------------------------------------------------------------------------------------