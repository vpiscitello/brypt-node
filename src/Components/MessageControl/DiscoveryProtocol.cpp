//------------------------------------------------------------------------------------------------
// File: DiscoveryProtocol.cpp
// Description: Used by the ExchangeProcessor to generate the application message required 
// after an exchange successfully completes. 
//------------------------------------------------------------------------------------------------
#include "DiscoveryProtocol.hpp"
#include "BryptMessage/ApplicationMessage.hpp"
#include "BryptMessage/MessageContext.hpp"
#include "Components/BryptPeer/BryptPeer.hpp"
#include "Components/Handler/HandlerDefinitions.hpp"
#include "Components/Handler/Connect.hpp"
#include "Utilities/NetworkUtils.hpp"
//------------------------------------------------------------------------------------------------
#include <lithium_json.hh>
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
//         "protocol": String
//         "entry": String,
//     },
//     ...
//     ],
// }

#ifndef LI_SYMBOL_entrypoints
#define LI_SYMBOL_entrypoints
LI_SYMBOL(entrypoints)
#endif
#ifndef LI_SYMBOL_entry
#define LI_SYMBOL_entry
LI_SYMBOL(entry)
#endif
#ifndef LI_SYMBOL_protocol
#define LI_SYMBOL_protocol
LI_SYMBOL(protocol)
#endif

//------------------------------------------------------------------------------------------------

DiscoveryProtocol::DiscoveryProtocol(
    Configuration::EndpointConfigurations const& configurations)
    : m_data(local::GenerateDiscoveryData(configurations))
{
}

//------------------------------------------------------------------------------------------------

bool DiscoveryProtocol::SendRequest(
    BryptIdentifier::SharedContainer const& spSourceIdentifier,
    std::shared_ptr<BryptPeer> const& spBryptPeer,
    MessageContext const& context) const
{
    assert(m_data.size() != 0);

    auto const spDestination = spBryptPeer->GetBryptIdentifier();
    if (!spDestination) {
        return false;
    }

    auto const optDiscoveryRequest = ApplicationMessage::Builder()
        .SetMessageContext(context)
        .SetSource(*spSourceIdentifier)
        .SetDestination(*spDestination)
        .SetCommand(local::Command, local::Phase)
        .SetPayload(m_data)
        .ValidatedBuild();
    assert(optDiscoveryRequest);

    return spBryptPeer->ScheduleSend(
        context.GetEndpointIdentifier(), optDiscoveryRequest->GetPack());
}

//------------------------------------------------------------------------------------------------

std::string local::GenerateDiscoveryData(
    Configuration::EndpointConfigurations const& configurations)
{
    auto request = li::mmm(
        s::entrypoints = {
            li::mmm(s::protocol = std::string(), s::entry = std::string()) });

    request.entrypoints.clear();

    for (auto const& options: configurations) {
        auto& entrypoint = request.entrypoints.emplace_back();
        entrypoint.protocol = options.GetProtocolName();

        auto const [primary, secondary] = options.GetBindingComponents();
        if (primary == "*") {
            entrypoint.entry.append(NetworkUtils::GetInterfaceAddress(options.GetInterface()));
        } else {
            entrypoint.entry.append(primary);
        }
        entrypoint.entry.append(NetworkUtils::ComponentSeperator);
        entrypoint.entry.append(secondary);
    }

    return li::json_encode(request);
}

//------------------------------------------------------------------------------------------------
