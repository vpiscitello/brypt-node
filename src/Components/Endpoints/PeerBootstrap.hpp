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
#include "../../Utilities/Message.hpp"
#include "../../Utilities/ReservedIdentifiers.hpp"
//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
namespace PeerBootstrap {
//------------------------------------------------------------------------------------------------

// Information used to generate the message required to request a connection with a Brypt peer
constexpr Command::Type ConnectionRequestCommand = Command::Type::Connect;
constexpr std::uint8_t ConnectionRequestPhase = static_cast<std::uint8_t>(Command::CConnectHandler::Phase::Discovery);
constexpr std::uint8_t ConnectionRequestNonce = 0;
constexpr std::string_view ConnectionRequestMessage = "Connection Request";

//------------------------------------------------------------------------------------------------
// Description: Generate a connection request message and callback a function with the message 
// intended to be sent. The caller must provide a valid callback function that has been bound
// with any required arguements, thus only requiring the message to be provided by the generator.
// This is done because different technologyies/endpoints may require different internal 
// information in their sending imeplementation.
//------------------------------------------------------------------------------------------------
template<typename Functor, 
            typename Enabled = std::enable_if_t<std::is_bind_expression_v<Functor>>>
auto SendContactMessage(
    Endpoints::EndpointIdType identifier,
    Endpoints::TechnologyType technology,
    NodeUtils::NodeIdType sourceIdentifier,
    Functor const& callback) -> typename Functor::result_type
{
    CMessage const message(
        {identifier, technology},
        sourceIdentifier, static_cast<NodeUtils::NodeIdType>(ReservedIdentifiers::Unknown),
        ConnectionRequestCommand, ConnectionRequestPhase,
        ConnectionRequestMessage, ConnectionRequestNonce);

    return callback(message.GetPack());
}

//------------------------------------------------------------------------------------------------
} // PeerBootstrap namespace
//------------------------------------------------------------------------------------------------
