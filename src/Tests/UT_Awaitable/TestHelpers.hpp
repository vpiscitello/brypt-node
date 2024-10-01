//----------------------------------------------------------------------------------------------------------------------
#include "Components/Awaitable/Definitions.hpp"
#include "Components/Identifier/BryptIdentifier.hpp"
#include "Components/Message/ApplicationMessage.hpp"
#include "Components/Peer/Proxy.hpp"
#include "Components/Network/Address.hpp"
#include "Components/Network/EndpointIdentifier.hpp"
#include "Components/Network/Protocol.hpp"
//----------------------------------------------------------------------------------------------------------------------
#include <cstdint>
#include <ranges>
#include <string_view>
//----------------------------------------------------------------------------------------------------------------------

//----------------------------------------------------------------------------------------------------------------------
namespace Awaitable::Test {
//----------------------------------------------------------------------------------------------------------------------

std::vector<Node::SharedIdentifier> GenerateIdentifiers(
    Node::SharedIdentifier const& spBaseIdentifier, std::size_t count);

std::optional<Message::Application::Parcel> GenerateRequest(
    Message::Context const& context, Node::Identifier const& source, Node::Identifier const& destination);

std::optional<Message::Application::Parcel> GenerateResponse(
    Message::Context const& context,
    Node::Identifier const& source,
    Node::Identifier const& destination,
    std::string_view route,
    Awaitable::TrackerKey const& key);

constexpr std::string_view RequestRoute = "/request";
constexpr std::string_view NoticeRoute = "/notice";
constexpr std::string_view Message = "Hello World!";

constexpr Network::Endpoint::Identifier EndpointIdentifier = 1;
constexpr Network::Protocol EndpointProtocol = Network::Protocol::TCP;

Network::RemoteAddress const RemoteClientAddress{ Network::Protocol::TCP, "127.0.0.1:35217", false };

constexpr Awaitable::TrackerKey TrackerKey = {
    0x01, 0x23, 0x45, 0x67, 0x89, 0xAB, 0xCD, 0xEF,
    0xEF, 0xCD, 0xAB, 0x89, 0x67, 0x45, 0x23, 0x01
};

//----------------------------------------------------------------------------------------------------------------------
} // Awaitable::Test namespace
//----------------------------------------------------------------------------------------------------------------------

inline std::vector<Node::SharedIdentifier> Awaitable::Test::GenerateIdentifiers(
    Node::SharedIdentifier const& spBaseIdentifier, std::size_t count)
{
    std::vector<Node::SharedIdentifier> identifiers;
    std::ranges::generate_n(std::back_insert_iterator(identifiers), count - 1, [] () mutable { 
        return std::make_shared<Node::Identifier const>(Node::GenerateIdentifier());
    });
    identifiers.emplace_back(spBaseIdentifier);
    return identifiers;
}

//----------------------------------------------------------------------------------------------------------------------

inline std::optional<Message::Application::Parcel> Awaitable::Test::GenerateRequest(
    Message::Context const& context, Node::Identifier const& source, Node::Identifier const& destination)
{
    return Message::Application::Parcel::GetBuilder()
        .SetContext(context)
        .SetSource(source)
        .SetDestination(destination)
        .SetRoute(RequestRoute)
        .SetPayload(Message)
        .BindExtension<Message::Extension::Awaitable>(
            Message::Extension::Awaitable::Request, TrackerKey)
        .ValidatedBuild();
}

//----------------------------------------------------------------------------------------------------------------------

inline std::optional<Message::Application::Parcel> Awaitable::Test::GenerateResponse(
    Message::Context const& context,
    Node::Identifier const& source,
    Node::Identifier const& destination,
    std::string_view route,
    Awaitable::TrackerKey const& key)
{
    return Message::Application::Parcel::GetBuilder()
        .SetContext(context)
        .SetSource(source)
        .SetDestination(destination)
        .SetRoute(route)
        .SetPayload(Message)
        .BindExtension<Message::Extension::Awaitable>(
            Message::Extension::Awaitable::Binding::Response, key)
        .ValidatedBuild();
}

//----------------------------------------------------------------------------------------------------------------------
