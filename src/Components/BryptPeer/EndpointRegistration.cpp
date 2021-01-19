//------------------------------------------------------------------------------------------------
// File: EndpointRegistration.cpp
// Description: 
//------------------------------------------------------------------------------------------------
#include "EndpointRegistration.hpp"
#include "Utilities/NetworkUtils.hpp"
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

EndpointRegistration::EndpointRegistration(
    Endpoints::EndpointIdType identifier,
    Endpoints::TechnologyType technology,
    MessageScheduler const& scheduler,
    std::string_view uri)
    : m_context(identifier, technology)
    , m_scheduler(scheduler)
    , m_entry(local::ParseEntryFromURI(uri))
{
}

//------------------------------------------------------------------------------------------------

MessageContext const& EndpointRegistration::GetMessageContext() const
{
    return m_context;
}

//------------------------------------------------------------------------------------------------

MessageContext& EndpointRegistration::GetWritableMessageContext()
{
    return m_context;
}

//------------------------------------------------------------------------------------------------

Endpoints::EndpointIdType CEndpointRegistration::GetEndpointIdentifier() const
{
    return m_context.GetEndpointIdentifier();
}

//------------------------------------------------------------------------------------------------

Endpoints::TechnologyType CEndpointRegistration::GetEndpointTechnology() const
{
    return m_context.GetEndpointTechnology();
}

//------------------------------------------------------------------------------------------------

MessageScheduler const& EndpointRegistration::GetScheduler() const
{
    return m_scheduler;
}

//------------------------------------------------------------------------------------------------

std::string const& EndpointRegistration::GetEntry() const
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
