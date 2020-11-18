//------------------------------------------------------------------------------------------------
// File: ConnectionDetails.hpp
// Description: 
//------------------------------------------------------------------------------------------------
#pragma once
//------------------------------------------------------------------------------------------------
#include "ConnectionState.hpp"
#include "../BryptPeer/BryptPeer.hpp"
#include "../../BryptIdentifier/BryptIdentifier.hpp"
#include "../../Utilities/TimeUtils.hpp"
//------------------------------------------------------------------------------------------------
#include <functional>
//------------------------------------------------------------------------------------------------

class CConnectionDetailsBase
{
public:
    explicit CConnectionDetailsBase(std::string_view uri)
        : m_uri(uri)
        , m_updateTimepoint()
        , m_sequenceNumber(0)
        , m_connectionState(ConnectionState::Resolving)
        , m_spBryptPeer()
    {
    }

    explicit CConnectionDetailsBase(std::shared_ptr<CBryptPeer> const& spBryptPeer)
        : m_uri()
        , m_updateTimepoint()
        , m_sequenceNumber(0)
        , m_connectionState(ConnectionState::Resolving)
        , m_spBryptPeer(spBryptPeer)
    {
    }

    std::string GetURI() const { return m_uri; }
    TimeUtils::Timepoint GetUpdateTimepoint() const { return m_updateTimepoint; }
    std::uint32_t GetMessageSequenceNumber() const { return m_sequenceNumber; }
    ConnectionState GetConnectionState() const { return m_connectionState; }
    std::shared_ptr<CBryptPeer> GetBryptPeer() const { return m_spBryptPeer; }
    BryptIdentifier::SharedContainer GetBryptIdentifier() const
    {
        if (!m_spBryptPeer) {
            return {};
        }
        return m_spBryptPeer->GetBryptIdentifier();
    }

    void SetURI(std::string_view uri)
    {
        m_uri = uri;
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

    void SetBryptPeer(std::shared_ptr<CBryptPeer> const& spBryptPeer)
    {
        m_spBryptPeer = spBryptPeer;
    }

    void Updated()
    {
        m_updateTimepoint = TimeUtils::GetSystemTimepoint();
    }

    bool HasAssociatedPeer() const 
    {
        if (!m_spBryptPeer) {
            return false;
        }

        return true;
    }

protected: 
    std::string m_uri;
    TimeUtils::Timepoint m_updateTimepoint;
    std::uint32_t m_sequenceNumber;
    ConnectionState m_connectionState;
    
    std::shared_ptr<CBryptPeer> m_spBryptPeer;

};

//------------------------------------------------------------------------------------------------

template <typename ExtensionType = void, typename Enabled = void>
class CConnectionDetails;

//------------------------------------------------------------------------------------------------

template <typename ExtensionType>
class CConnectionDetails<ExtensionType, std::enable_if_t<
    std::is_same_v<ExtensionType, void>>> : public CConnectionDetailsBase
{
public:
    using CConnectionDetailsBase::CConnectionDetailsBase;

    CConnectionDetails(CConnectionDetails const& other) = default;
    CConnectionDetails(CConnectionDetails&& other) = default;

    CConnectionDetails& operator=(CConnectionDetails const& other)
    {
        if (other.m_uri.size() != 0) {
            m_uri = other.m_uri;
        }
        m_updateTimepoint = other.m_updateTimepoint;
        m_sequenceNumber = other.m_sequenceNumber;
        m_connectionState = other.m_connectionState;
        m_spBryptPeer = other.m_spBryptPeer;
        return *this;
    }

};

//------------------------------------------------------------------------------------------------

template <typename ExtensionType>
class CConnectionDetails<ExtensionType, std::enable_if_t<
    std::is_class_v<ExtensionType>>> : public CConnectionDetailsBase
{
public:
    using CConnectionDetailsBase::CConnectionDetailsBase;

    using ReadExtensionFunction = std::function<void(ExtensionType const&)>;
    using UpdateExtensionFunction = std::function<void(ExtensionType&)>;

    CConnectionDetails(
        BryptIdentifier::SharedContainer const& spBryptIdentifier,
        ExtensionType const& extension)
        : CConnectionDetailsBase(spBryptIdentifier)
        , m_extension(extension)
    {
    }

    CConnectionDetails(
        std::string_view uri,
        ExtensionType const& extension)
        : CConnectionDetailsBase(uri)
        , m_extension(extension)
    {
    }

    CConnectionDetails(CConnectionDetails const& other) = default;
    CConnectionDetails(CConnectionDetails&& other) = default;

    CConnectionDetails& operator=(CConnectionDetails const& other)
    {
        if (other.m_uri.size() != 0) {
            m_uri = other.m_uri;
        }
        m_updateTimepoint = other.m_updateTimepoint;
        m_sequenceNumber = other.m_sequenceNumber;
        m_connectionState = other.m_connectionState;
        m_spBryptPeer = other.spBryptPeer;
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

//------------------------------------------------------------------------------------------------