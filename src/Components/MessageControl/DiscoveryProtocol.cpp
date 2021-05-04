//----------------------------------------------------------------------------------------------------------------------
// File: DiscoveryProtocol.cpp
// Description: Used by the ExchangeProcessor to generate the application message required 
// after an exchange successfully completes. 
//----------------------------------------------------------------------------------------------------------------------
#include "DiscoveryProtocol.hpp"
#include "BryptMessage/ApplicationMessage.hpp"
#include "BryptMessage/MessageContext.hpp"
#include "Components/Handler/HandlerDefinitions.hpp"
#include "Components/Handler/Connect.hpp"
#include "Components/Network/Address.hpp"
#include "Components/Peer/Proxy.hpp"
//----------------------------------------------------------------------------------------------------------------------
#include <lithium_json.hh>
//----------------------------------------------------------------------------------------------------------------------
#include <cassert>
#include <string_view>
#include <vector>
//----------------------------------------------------------------------------------------------------------------------

//----------------------------------------------------------------------------------------------------------------------
namespace {
namespace local {
//----------------------------------------------------------------------------------------------------------------------

constexpr Handler::Type Handler = Handler::Type::Connect;
constexpr std::uint8_t Phase = static_cast<std::uint8_t>(Handler::Connect::Phase::Discovery);

std::string GenerateDiscoveryData(Configuration::EndpointsSet const& endpoints);

//----------------------------------------------------------------------------------------------------------------------
} // local namespace
} // namespace
//----------------------------------------------------------------------------------------------------------------------

//----------------------------------------------------------------------------------------------------------------------
// Description: Symbol loading for JSON encoding
//----------------------------------------------------------------------------------------------------------------------

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

//----------------------------------------------------------------------------------------------------------------------

DiscoveryProtocol::DiscoveryProtocol(Configuration::EndpointsSet const& endpoints)
    : m_data(local::GenerateDiscoveryData(endpoints))
{
}

//----------------------------------------------------------------------------------------------------------------------

bool DiscoveryProtocol::SendRequest(
    Node::SharedIdentifier const& spSourceIdentifier,
    std::shared_ptr<Peer::Proxy> const& spPeerProxy,
    MessageContext const& context) const
{
    assert(m_data.size() != 0);

    auto const spDestination = spPeerProxy->GetNodeIdentifier();
    if (!spDestination) { return false; }

    auto const optDiscoveryRequest = ApplicationMessage::Builder()
        .SetMessageContext(context)
        .SetSource(*spSourceIdentifier)
        .SetDestination(*spDestination)
        .SetCommand(local::Handler, local::Phase)
        .SetPayload(m_data)
        .ValidatedBuild();
    assert(optDiscoveryRequest);

    return spPeerProxy->ScheduleSend(context.GetEndpointIdentifier(), optDiscoveryRequest->GetPack());
}

//----------------------------------------------------------------------------------------------------------------------

std::string local::GenerateDiscoveryData(Configuration::EndpointsSet const& endpoints)
{
    auto request = li::mmm(
        s::entrypoints = {
            li::mmm(s::protocol = std::string(), s::entry = std::string()) });

    request.entrypoints.clear();

    for (auto const& options: endpoints) {
        auto& entrypoint = request.entrypoints.emplace_back();
        entrypoint.protocol = options.GetProtocolName();
        entrypoint.entry = options.GetBinding().GetUri();
    }

    return li::json_encode(request);
}

//----------------------------------------------------------------------------------------------------------------------
