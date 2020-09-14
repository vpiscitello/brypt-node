//------------------------------------------------------------------------------------------------
// File: EndpointRegistration.cpp
// Description: 
//------------------------------------------------------------------------------------------------
#include "EndpointRegistration.hpp"
#include "../../Utilities/NetworkUtils.hpp"
//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
namespace {
namespace local {
//------------------------------------------------------------------------------------------------

std::string ParseEntryFromURI(std::string_view uri);

//------------------------------------------------------------------------------------------------
} // local namespace
} // namespace
//------------------------------------------------------------------------------------------------

CEndpointRegistration::CEndpointRegistration(
    Endpoints::EndpointIdType identifier,
    Endpoints::TechnologyType technology,
    MessageScheduler const& scheduler,
    std::string_view uri)
    : m_identifier(identifier)
    , m_technology(technology)
    , m_scheduler(scheduler)
    , m_entry(local::ParseEntryFromURI(uri))
{
}

//------------------------------------------------------------------------------------------------

Endpoints::EndpointIdType CEndpointRegistration::GetEndpointIdentifier() const
{
    return m_identifier;
}

//------------------------------------------------------------------------------------------------

Endpoints::TechnologyType CEndpointRegistration::GetEndpointTechnology() const
{
    return m_technology;
}

//------------------------------------------------------------------------------------------------

MessageScheduler const& CEndpointRegistration::GetScheduler() const
{
    return m_scheduler;
}

//------------------------------------------------------------------------------------------------

std::string const& CEndpointRegistration::GetEntry() const
{
    return m_entry;
}

//------------------------------------------------------------------------------------------------

std::string local::ParseEntryFromURI(std::string_view uri) {
    if (auto const pos = uri.find(NetworkUtils::SchemeSeperator); pos != std::string::npos) {
        return std::string(uri.begin() + pos + NetworkUtils::SchemeSeperator.size(), uri.end());
    } else {
        return std::string(uri.begin(), uri.end());
    }
}

//------------------------------------------------------------------------------------------------
