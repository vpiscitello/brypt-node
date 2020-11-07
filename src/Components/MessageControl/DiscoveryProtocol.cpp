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

std::string GenerateDiscoveryData(Configuration::EndpointConfigurations const& configurations);

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

CDiscoveryProtocol::CDiscoveryProtocol(
    Configuration::EndpointConfigurations const& configurations)
    : m_data(local::GenerateDiscoveryData(configurations))
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
        .SetPayload(m_data)
        .ValidatedBuild();
    assert(optDiscoveryRequest);

    return spBryptPeer->ScheduleSend(context, optDiscoveryRequest->GetPack());
}

//------------------------------------------------------------------------------------------------

std::string local::GenerateDiscoveryData(
    Configuration::EndpointConfigurations const& configurations)
{
    auto request = iod::make_metamap(
        s::entrypoints = {
            iod::make_metamap(
                s::technology = std::string(), s::entry = std::string()) });

    for (auto const& options: configurations) {
        auto& entrypoint = request.entrypoints.emplace_back();
        entrypoint.technology = options.GetTechnologyName();
        entrypoint.entry = options.GetBinding();
    }

    return iod::json_encode(request);
}

//------------------------------------------------------------------------------------------------
