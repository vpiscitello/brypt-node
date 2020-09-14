//------------------------------------------------------------------------------------------------
// File: BryptPeer.hpp
// Description: 
//------------------------------------------------------------------------------------------------
#pragma once
//------------------------------------------------------------------------------------------------
#include "EndpointRegistration.hpp"
#include "../Endpoints/EndpointIdentifier.hpp"
#include "../Endpoints/MessageScheduler.hpp"
#include "../Endpoints/TechnologyType.hpp"
//------------------------------------------------------------------------------------------------
#include <memory>
#include <mutex>
#include <string>
#include <string_view>
#include <unordered_map>
//------------------------------------------------------------------------------------------------

namespace BryptIdentifier {
    class CContainer;
    using SharedContainer = std::shared_ptr<CContainer>;
}

class CMessage;

//------------------------------------------------------------------------------------------------

class CBryptPeer
{
public:
    CBryptPeer();

    explicit CBryptPeer(BryptIdentifier::CContainer const& identifier);

    BryptIdentifier::SharedContainer GetBryptIdentifier() const;
    std::string GetLocation() const;
    std::optional<std::string> GetRegisteredEntry(Endpoints::TechnologyType technology);

    void SetLocation(std::string_view location);

    void RegisterEndpointConnection(
        Endpoints::EndpointIdType identifier,
        Endpoints::TechnologyType technology,
        MessageScheduler const& scheduler = {},
        std::string_view uri = {});
    void WithdrawEndpointConnection(Endpoints::EndpointIdType identifier);
    std::uint32_t RegisteredEndpointCount() const;

    bool ScheduleSend(CMessage const& message) const;

private:
    using RegisteredEndpoints = std::unordered_map<Endpoints::EndpointIdType, CEndpointRegistration>;

    mutable std::recursive_mutex m_mutex;

    BryptIdentifier::SharedContainer const m_spBryptIdentifier;
    std::string m_location;

    RegisteredEndpoints m_endpoints;
};

//------------------------------------------------------------------------------------------------
