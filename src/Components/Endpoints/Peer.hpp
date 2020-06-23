//------------------------------------------------------------------------------------------------
// File: PeerInformation.hpp
// Description: 
//------------------------------------------------------------------------------------------------
#pragma once
//------------------------------------------------------------------------------------------------
#include "TechnologyType.hpp"
#include "../../Utilities/NodeUtils.hpp"
#include "../../Utilities/ReservedIdentifiers.hpp"
//------------------------------------------------------------------------------------------------

class CPeer
{
public:
    CPeer()
        : m_id(static_cast<NodeUtils::NodeIdType>(ReservedIdentifiers::Unknown))
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
    NodeUtils::NodeIdType m_id;
    Endpoints::TechnologyType m_technology;
    std::string m_entry;
    std::string m_location;

};

//------------------------------------------------------------------------------------------------