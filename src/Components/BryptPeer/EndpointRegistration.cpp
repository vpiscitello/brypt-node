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
    Network::Endpoint::Identifier identifier,
    Network::Protocol protocol,
    MessageScheduler const& scheduler,
    std::string_view uri)
    : m_context(identifier, protocol)
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

Network::Endpoint::Identifier EndpointRegistration::GetEndpointIdentifier() const
{
    return m_context.GetEndpointIdentifier();
}

//------------------------------------------------------------------------------------------------

Network::Protocol EndpointRegistration::GetEndpointProtocol() const
{
    return m_context.GetEndpointProtocol();
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
