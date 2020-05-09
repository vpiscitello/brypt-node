//------------------------------------------------------------------------------------------------
// File: PeerInformation.hpp
// Description: 
//------------------------------------------------------------------------------------------------
#pragma once
//------------------------------------------------------------------------------------------------
#include "TechnologyType.hpp"
#include "../../Utilities/NodeUtils.hpp"
//------------------------------------------------------------------------------------------------

class CPeer
{
public:
    CPeer()
        : m_id(0)
        , m_technology(Endpoints::TechnologyType::Invalid)
        , m_entry()
        , m_location()
    {
    }

    CPeer(
        NodeUtils::NodeIdType id,
        Endpoints::TechnologyType technology,
        std::string_view entry,
        std::string_view location = "")
        : m_id(id)
        , m_technology(technology)
        , m_entry(entry)
        , m_location(location)
    {
    }

    CPeer(CPeer const& other)
        : m_id(other.m_id)
        , m_technology(other.m_technology)
        , m_entry(other.m_entry)
        , m_location(other.m_location)
    {
    }

    NodeUtils::NodeIdType GetNodeId() const
    {
        return m_id;
    }

    Endpoints::TechnologyType GetTechnologyType() const
    {
        return m_technology;
    }

    std::string GetEntry() const
    {
        return m_entry;
    }

    std::string GetLocation() const
    {
        return m_location;
    }

    void SetLocation(std::string_view location)
    {
        m_location = location;
    }

private:
    NodeUtils::NodeIdType const m_id;
    Endpoints::TechnologyType const m_technology;
    std::string const m_entry;
    std::string m_location;

};

//------------------------------------------------------------------------------------------------