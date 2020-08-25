//------------------------------------------------------------------------------------------------
// File: PeerInformation.hpp
// Description: 
//------------------------------------------------------------------------------------------------
#pragma once
//------------------------------------------------------------------------------------------------
#include "TechnologyType.hpp"
#include "../../BryptIdentifier/BryptIdentifier.hpp"
#include "../../BryptIdentifier/ReservedIdentifiers.hpp"
#include "../../Utilities/NetworkUtils.hpp"
//------------------------------------------------------------------------------------------------

class CPeer
{
public:
    CPeer()
        : m_identifier()
        , m_technology(Endpoints::TechnologyType::Invalid)
        , m_scheme()
        , m_entry()
        , m_location()
    {
    }

    CPeer(
        BryptIdentifier::CContainer const& identifier,
        Endpoints::TechnologyType technology,
        std::string_view uri,
        std::string_view location = "")
        : m_identifier(identifier)
        , m_technology(technology)
        , m_scheme()
        , m_entry()
        , m_location(location)
    {
        ParseURI(uri);
    }

    CPeer(
        std::string_view identifier,
        Endpoints::TechnologyType technology,
        std::string_view uri,
        std::string_view location = "")
        : m_identifier(identifier)
        , m_technology(technology)
        , m_scheme()
        , m_entry()
        , m_location(location)
    {
        ParseURI(uri);
    }

    BryptIdentifier::CContainer GetIdentifier() const
    {
        return m_identifier;
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

    void SetIdentifier(BryptIdentifier::CContainer const& identifier)
    {
        m_identifier = identifier;
    }

    void SetLocation(std::string_view location)
    {
        m_location = location;
    }

private:
    void ParseURI(std::string_view uri) {
        if (auto const pos = uri.find(NetworkUtils::SchemeSeperator); pos != std::string::npos) {
            m_scheme = std::string(uri.begin(), uri.begin() + pos + NetworkUtils::SchemeSeperator.size());
            m_entry = std::string(uri.begin() + pos + NetworkUtils::SchemeSeperator.size(), uri.end());
        } else {
            m_entry = uri;
        }
    }

    BryptIdentifier::CContainer m_identifier;
    Endpoints::TechnologyType m_technology;
    std::string m_scheme;
    std::string m_entry;
    std::string m_location;

};

//------------------------------------------------------------------------------------------------