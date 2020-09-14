//------------------------------------------------------------------------------------------------
// File: BryptPeer.cpp
// Description: 
//------------------------------------------------------------------------------------------------
#include "BryptPeer.hpp"
#include "../../BryptIdentifier/BryptIdentifier.hpp"
#include "../../BryptIdentifier/ReservedIdentifiers.hpp"
#include "../../Message/Message.hpp"
#include "../../Utilities/NetworkUtils.hpp"
//------------------------------------------------------------------------------------------------

CBryptPeer::CBryptPeer()
    : m_mutex()
    , m_spBryptIdentifier()
    , m_location()
    , m_endpoints()
{
}

//------------------------------------------------------------------------------------------------

CBryptPeer::CBryptPeer(BryptIdentifier::CContainer const& identifier)
    : m_mutex()
    , m_spBryptIdentifier(std::make_shared<BryptIdentifier::CContainer>(identifier))
    , m_location()
    , m_endpoints()
{
}

//------------------------------------------------------------------------------------------------

BryptIdentifier::SharedContainer CBryptPeer::GetBryptIdentifier() const
{
    return m_spBryptIdentifier;
}

//------------------------------------------------------------------------------------------------

std::string CBryptPeer::GetLocation() const
{
    std::scoped_lock lock(m_mutex);
    return m_location;
}

//------------------------------------------------------------------------------------------------

std::optional<std::string> CBryptPeer::GetRegisteredEntry(Endpoints::TechnologyType technology)
{
    std::scoped_lock lock(m_mutex);
    for (auto const& [identifier, endpoint]: m_endpoints) {
        if (endpoint.GetEndpointTechnology() == technology) {
            if (auto const entry = endpoint.GetEntry(); !entry.empty()) {
                return entry;
            }
        }
    }

    return {};
}

//------------------------------------------------------------------------------------------------

void CBryptPeer::SetLocation(std::string_view location)
{
    std::scoped_lock lock(m_mutex);
    m_location = location;
}

//------------------------------------------------------------------------------------------------

void CBryptPeer::RegisterEndpointConnection(
    Endpoints::EndpointIdType identifier,
    Endpoints::TechnologyType technology,
    MessageScheduler const& scheduler,
    std::string_view uri)
{
    std::scoped_lock lock(m_mutex);
    m_endpoints.try_emplace(
        // Registered Endpoint Key
        identifier,
        // Registered Endpoint Arguments
        identifier, technology, scheduler, uri);
}

//------------------------------------------------------------------------------------------------

void CBryptPeer::WithdrawEndpointConnection(Endpoints::EndpointIdType identifier)
{
    std::scoped_lock lock(m_mutex);
    m_endpoints.erase(identifier);
}

//------------------------------------------------------------------------------------------------

std::uint32_t CBryptPeer::RegisteredEndpointCount() const
{
    std::scoped_lock lock(m_mutex);
    return m_endpoints.size();
}

//------------------------------------------------------------------------------------------------

bool CBryptPeer::ScheduleSend(CMessage const& message) const
{
    std::scoped_lock lock(m_mutex);
    auto const& context = message.GetMessageContext();
    if (auto const itr = m_endpoints.find(context.GetEndpointIdentifier());
        itr != m_endpoints.end()) {
        auto const& [identifier, endpoint] = *itr;
        auto const& scheduler = endpoint.GetScheduler();
        if (scheduler) {
            return scheduler(message);
        }
    }

    return false;
}

//------------------------------------------------------------------------------------------------
