//------------------------------------------------------------------------------------------------
// File: PeerConnector.hpp
// Description: Used by endpoints to generate the message needed to start a valid and 
// authenticated connection with a Brypt peer. The intention is to avoid asking the core
//  application how it should procede after a raw connection has been established over a given 
// technology. Instead, endpoints provides a function that has been bound with any required 
// arguements (via std::bind) that will be called with the message after it has been geenrated.
// Avoiding the time it takes to notify the core and cycle to the receive the response in the 
// event queue. 
//------------------------------------------------------------------------------------------------
#pragma once
//------------------------------------------------------------------------------------------------
#include <cstdint>
#include <string_view>
#include <type_traits>
#include <functional>
//------------------------------------------------------------------------------------------------
#include "EndpointIdentifier.hpp"
#include "TechnologyType.hpp"
#include "../../Components/Command/CommandDefinitions.hpp"
#include "../../Components/Command/ConnectHandler.hpp"
#include "../../Message/Message.hpp"
#include "../../Message/MessageBuilder.hpp"
#include "../../Utilities/ReservedIdentifiers.hpp"
//------------------------------------------------------------------------------------------------
#include "../../Libraries/metajson/metajson.hh"
#include <cassert>
#include <string>
#include <vector>
//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
namespace PeerBootstrap {
//------------------------------------------------------------------------------------------------

// Information used to generate the message required to request a connection with a Brypt peer
constexpr Command::Type ConnectCommand = Command::Type::Connect;
constexpr std::uint8_t DiscoveryPhase = static_cast<std::uint8_t>(Command::CConnectHandler::Phase::Discovery);
constexpr std::uint8_t InitialNonce = 0;

template<typename Functor, typename Enabled = std::enable_if_t<std::is_bind_expression_v<Functor>>>
auto SendContactMessage(
    IEndpointMediator const* const pEndpointMediator,
    Endpoints::EndpointIdType identifier,
    Endpoints::TechnologyType technology,
    NodeUtils::NodeIdType sourceIdentifier,
    Functor const& callback) -> typename Functor::result_type;

//------------------------------------------------------------------------------------------------
namespace Json {
//------------------------------------------------------------------------------------------------

struct TTechnologyEntry
{
    TTechnologyEntry()
        : name()
        , entry()
    {
    }

    TTechnologyEntry(std::string_view name, std::string_view entry)
        : name(name)
        , entry(entry)
    {
    }

    std::string name;
    std::string entry;
};
using TechnologyEntries = std::vector<TTechnologyEntry>;

struct TConnectRequest;

//------------------------------------------------------------------------------------------------
} // Json namespace
//------------------------------------------------------------------------------------------------
} // PeerBootstrap namespace
//------------------------------------------------------------------------------------------------

struct PeerBootstrap::Json::TConnectRequest
{
    TConnectRequest()
        : entrypoints()
    {
    }

    TConnectRequest(TechnologyEntries const& entrypoints)
        : entrypoints(entrypoints)
    {
    }

    TechnologyEntries entrypoints;
};

//------------------------------------------------------------------------------------------------
// Description: Symbol loading for JSON encoding
//------------------------------------------------------------------------------------------------

// {
//     "entrypoints": [
//     {
//         "name": String
//         "entries": String,
//     },
//     ...
//     ],
// }

IOD_SYMBOL(entrypoints)
IOD_SYMBOL(name)
IOD_SYMBOL(entry)

//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
// Description: Generate a connection request message and callback a function with the message 
// intended to be sent. The caller must provide a valid callback function that has been bound
// with any required arguements, thus only requiring the message to be provided by the generator.
// This is done because different technologyies/endpoints may require different internal 
// information in their sending imeplementation.
//------------------------------------------------------------------------------------------------
template<typename Functor, typename Enabled = std::enable_if_t<std::is_bind_expression_v<Functor>>>
auto PeerBootstrap::SendContactMessage(
    IEndpointMediator const* const pEndpointMediator,
    Endpoints::EndpointIdType identifier,
    Endpoints::TechnologyType technology,
    NodeUtils::NodeIdType source,
    Functor const& callback) -> typename Functor::result_type
{
    Json::TConnectRequest request;
    if (pEndpointMediator) {
        auto const entries = pEndpointMediator->GetEndpointEntries();
        for (auto const& [technology, entry]: entries) {
            request.entrypoints.emplace_back(
                Json::TTechnologyEntry(Endpoints::TechnologyTypeToString(technology), entry));
        }
    }

    std::string const encoded = iod::json_object(
        s::entrypoints = iod::json_vector(
            s::name,
            s::entry
        )).encode(request);

    OptionalMessage const optDiscoveryRequest = CMessage::Builder()
        .SetMessageContext({ identifier, technology })
        .SetSource(source)
        .SetDestination(ReservedIdentifiers::Unknown)
        .SetCommand(ConnectCommand, DiscoveryPhase)
        .SetData(encoded, InitialNonce)
        .ValidatedBuild();
    assert(optDiscoveryRequest);

    return callback(optDiscoveryRequest->GetPack());
}

//------------------------------------------------------------------------------------------------
