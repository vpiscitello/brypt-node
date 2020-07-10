//------------------------------------------------------------------------------------------------
// File: PeerInformation.hpp
// Description: 
//------------------------------------------------------------------------------------------------
#pragma once
//------------------------------------------------------------------------------------------------
#include "TechnologyType.hpp"
#include "../../Utilities/NetworkUtils.hpp"
#include "../../Utilities/NodeUtils.hpp"
#include "../../Utilities/ReservedIdentifiers.hpp"
//------------------------------------------------------------------------------------------------

class CPeer
{
public:
    CPeer()
        : m_id(static_cast<NodeUtils::NodeIdType>(ReservedIdentifiers::Invalid))
        , m_technology(Endpoints::TechnologyType::Invalid)
        , m_scheme()
        , m_entry()
        , m_location()
    {
    }

    CPeer(
        NodeUtils::NodeIdType id,
        Endpoints::TechnologyType technology,
        std::string_view uri,
        std::string_view location = "")
        : m_id(id)
        , m_technology(technology)
        , m_scheme()
        , m_entry()
        , m_location(location)
    {
        if (auto const pos = uri.find(NetworkUtils::SchemeSeperator); pos != std::string::npos) {
            m_scheme = std::string(uri.begin(), uri.begin() + pos + NetworkUtils::SchemeSeperator.size());
            m_entry = std::string(uri.begin() + pos + NetworkUtils::SchemeSeperator.size(), uri.end());
        } else {
            m_entry = uri;
        }
    }

    NodeUtils::NodeIdType GetNodeId() const
    {
        return m_id;
    }

    Endpoints::TechnologyType GetTechnologyType() const
    {
        return m_technology;
    }

    std::string GetURI() const
    {
        return m_scheme + m_entry;
    }

    std::string GetEntry() const
    {
        return m_entry;
    }

    std::string GetLocation() const
    {
        return m_location;
    }

    void SetNodeId(NodeUtils::NodeIdType id)
    {
        m_id = id;
    }

    void SetLocation(std::string_view location)
    {
        m_location = location;
    }

private:
    NodeUtils::NodeIdType m_id;
    Endpoints::TechnologyType m_technology;
    std::string m_scheme;
    std::string m_entry;
    std::string m_location;

};

//------------------------------------------------------------------------------------------------