//------------------------------------------------------------------------------------------------
// File: DiscoveryProtocol.cpp
// Description: Used by the ExchangeProcessor to generate the application message required 
// after an exchange successfully completes. 
//------------------------------------------------------------------------------------------------
#include "DiscoveryProtocol.hpp"
#include "../BryptPeer/BryptPeer.hpp"
#include "../Command/CommandDefinitions.hpp"
#include "../Command/ConnectHandler.hpp"
#include "../../BryptMessage/ApplicationMessage.hpp"
#include "../../BryptMessage/MessageContext.hpp"
//------------------------------------------------------------------------------------------------
#include "../../Libraries/metajson/metajson.hh"
//------------------------------------------------------------------------------------------------
#include <cassert>
#include <string_view>
#include <vector>
//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
namespace {
namespace local {
//------------------------------------------------------------------------------------------------

constexpr Command::Type Command = Command::Type::Connect;
constexpr std::uint8_t Phase = static_cast<std::uint8_t>(
    Command::CConnectHandler::Phase::Discovery);

std::string GenerateDiscoveryData(IEndpointMediator const* const pEndpointMediator);

//------------------------------------------------------------------------------------------------
} // local namespace
} // namespace
//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
// Description: Symbol loading for JSON encoding
//------------------------------------------------------------------------------------------------

// DiscoveryRequest {
//     "entrypoints": [
//     {
//         "technology": String
//         "entry": String,
//     },
//     ...
//     ],
// }

IOD_SYMBOL(entrypoints)
IOD_SYMBOL(entry)
IOD_SYMBOL(technology)

//------------------------------------------------------------------------------------------------

CDiscoveryProtocol::CDiscoveryProtocol(IEndpointMediator const* const pEndpointMediator)
    : m_data(local::GenerateDiscoveryData(pEndpointMediator))
{
}

//------------------------------------------------------------------------------------------------

bool CDiscoveryProtocol::SendRequest(
    BryptIdentifier::SharedContainer const& spSourceIdentifier,
    std::shared_ptr<CBryptPeer> const& spBryptPeer,
    CMessageContext const& context) const
{
    assert(m_data.size() != 0);

    auto const optDiscoveryRequest = CApplicationMessage::Builder()
        .SetMessageContext(context)
        .SetSource(*spSourceIdentifier)
        .SetCommand(local::Command, local::Phase)
        .SetData(m_data)
        .ValidatedBuild();
    assert(optDiscoveryRequest);

    return spBryptPeer->ScheduleSend(context, optDiscoveryRequest->GetPack());
}

//------------------------------------------------------------------------------------------------

std::string local::GenerateDiscoveryData(IEndpointMediator const* const pEndpointMediator)
{
    if (!pEndpointMediator) {
        return {};
    }

    auto request = iod::make_metamap(
        s::entrypoints = {
            iod::make_metamap(
                s::technology = std::string(), s::entry = std::string()) });

    if (pEndpointMediator) {
        auto const entries = pEndpointMediator->GetEndpointEntries();
        for (auto const& [technology, entry]: entries) {
            auto& entrypoint = request.entrypoints.emplace_back();
            entrypoint.technology = Endpoints::TechnologyTypeToString(technology);
            entrypoint.entry = entry;
        }
    }

    return iod::json_encode(request);
}

//------------------------------------------------------------------------------------------------
