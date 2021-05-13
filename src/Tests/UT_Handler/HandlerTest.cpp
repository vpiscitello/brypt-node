//----------------------------------------------------------------------------------------------------------------------
#include "BryptIdentifier/BryptIdentifier.hpp"
#include "BryptMessage/ApplicationMessage.hpp"
#include "BryptNode/BryptNode.hpp"
#include "Components/Configuration/Configuration.hpp"
#include "Components/Configuration/Manager.hpp"
#include "Components/Configuration/PeerPersistor.hpp"
#include "Components/Handler/Handler.hpp"
#include "Components/Network/EndpointIdentifier.hpp"
#include "Components/Network/Protocol.hpp"
#include "Utilities/NodeUtils.hpp"
//----------------------------------------------------------------------------------------------------------------------
#include <gtest/gtest.h>
//----------------------------------------------------------------------------------------------------------------------
#include <cstdint>
#include <chrono>
#include <set>
#include <string>
#include <string_view>
//----------------------------------------------------------------------------------------------------------------------

//----------------------------------------------------------------------------------------------------------------------
namespace {
namespace local {
//----------------------------------------------------------------------------------------------------------------------

void SetupHandlerMap(Handler::Map& handlers, BryptNode& node);
MessageContext GenerateMessageContext();

//----------------------------------------------------------------------------------------------------------------------
} // local namespace
//----------------------------------------------------------------------------------------------------------------------
namespace test {
//----------------------------------------------------------------------------------------------------------------------

Configuration::EndpointOptions CreateEndpointOptions();
std::unique_ptr<Configuration::Manager> CreateConfigurationManager();

Node::Identifier const ClientIdentifier(Node::GenerateIdentifier());
auto const spServerIdentifier = std::make_shared<Node::Identifier const>(Node::GenerateIdentifier());

constexpr std::string_view ProtocolName = "TCP";
constexpr Network::Protocol ProtocolType = Network::Protocol::TCP;
constexpr std::string_view Interface = "lo";
constexpr std::string_view ServerBinding = "*:35216";
constexpr std::string_view ClientBinding = "*:35217";
constexpr std::string_view ServerEntry = "127.0.0.1:35216";
constexpr std::string_view ClientEntry = "127.0.0.1:35217";

constexpr std::uint8_t BasePhase = 0;
constexpr std::string_view Message = "Hello World!";

constexpr Network::Endpoint::Identifier const EndpointIdentifier = 1;
constexpr Network::Protocol const EndpointProtocol = Network::Protocol::TCP;

//----------------------------------------------------------------------------------------------------------------------
} // local namespace
} // namespace
//----------------------------------------------------------------------------------------------------------------------

// TODO: Add tests for handler sequences using connections

TEST(HandlerSuite, HandlerMatchingTest)
{
    auto const upConfiguration = test::CreateConfigurationManager();

    // The node itself will set up internal handlers that can operate on it's internal state, but in order to setup our 
    // own we need to provide the handlers a node instance and a state.
    BryptNode node(upConfiguration, nullptr, nullptr, nullptr, nullptr, nullptr);

    Handler::Map handlers;
    local::SetupHandlerMap(handlers, node);

    MessageContext const context = local::GenerateMessageContext();

    auto const optConnectRequest = ApplicationMessage::Builder()
        .SetMessageContext(context)
        .SetSource(test::ClientIdentifier)
        .SetDestination(*test::spServerIdentifier)
        .SetCommand(Handler::Type::Connect, test::BasePhase)
        .SetPayload(test::Message)
        .ValidatedBuild();
    
    auto const connectHandlerItr = handlers.find(optConnectRequest->GetCommand());
    ASSERT_NE(connectHandlerItr, handlers.end());

    auto const connectHandlerReturnType = connectHandlerItr->second->GetType();
    EXPECT_EQ(connectHandlerReturnType, Handler::Type::Connect);

    auto const optElectionRequest = ApplicationMessage::Builder()
        .SetMessageContext(context)
        .SetSource(test::ClientIdentifier)
        .SetDestination(*test::spServerIdentifier)
        .SetCommand(Handler::Type::Election, test::BasePhase)
        .SetPayload(test::Message)
        .ValidatedBuild();

    auto const electionHandlerItr = handlers.find(optElectionRequest->GetCommand());
    ASSERT_NE(electionHandlerItr, handlers.end());

    auto const electionHandlerReturnType = electionHandlerItr->second->GetType();
    EXPECT_EQ(electionHandlerReturnType, Handler::Type::Election);

    auto const optInformationRequest = ApplicationMessage::Builder()
        .SetMessageContext(context)
        .SetSource(test::ClientIdentifier)
        .SetDestination(*test::spServerIdentifier)
        .SetCommand(Handler::Type::Information, test::BasePhase)
        .SetPayload(test::Message)
        .ValidatedBuild();
    
    auto const informationHandlerItr = handlers.find(optInformationRequest->GetCommand());
    ASSERT_NE(informationHandlerItr, handlers.end());

    auto const informationHandlerReturnType = informationHandlerItr->second->GetType();
    EXPECT_EQ(informationHandlerReturnType, Handler::Type::Information);

    auto const optQueryRequest = ApplicationMessage::Builder()
        .SetMessageContext(context)
        .SetSource(test::ClientIdentifier)
        .SetDestination(*test::spServerIdentifier)
        .SetCommand(Handler::Type::Query, test::BasePhase)
        .SetPayload(test::Message)
        .ValidatedBuild();
    
    auto const queryHandlerItr = handlers.find(optQueryRequest->GetCommand());
    ASSERT_NE(queryHandlerItr, handlers.end());

    auto const queryHandlerReturnType = queryHandlerItr->second->GetType();
    EXPECT_EQ(queryHandlerReturnType, Handler::Type::Query);
}

//----------------------------------------------------------------------------------------------------------------------

Configuration::EndpointOptions test::CreateEndpointOptions()
{
    Configuration::EndpointOptions options(
        test::ProtocolType,
        test::Interface,
        test::ServerBinding);

    return options;
}

//----------------------------------------------------------------------------------------------------------------------

std::unique_ptr<Configuration::Manager> test::CreateConfigurationManager()
{
    auto const endpointOptions = CreateEndpointOptions();
    Configuration::Settings settings(
        Configuration::DetailsOptions("test-node"),
        {endpointOptions},
        Configuration::SecurityOptions()
    );

    return std::make_unique<Configuration::Manager>(settings);
}

//----------------------------------------------------------------------------------------------------------------------

void local::SetupHandlerMap(
    Handler::Map& handlers, BryptNode& node)
{
    handlers.emplace(
        Handler::Type::Connect,
        Handler::Factory(Handler::Type::Connect, node));

    handlers.emplace(
        Handler::Type::Election,
        Handler::Factory(Handler::Type::Election, node));

    handlers.emplace(
        Handler::Type::Information,
        Handler::Factory(Handler::Type::Information, node));
  
    handlers.emplace(
        Handler::Type::Query,
        Handler::Factory(Handler::Type::Query, node));

}

//----------------------------------------------------------------------------------------------------------------------

MessageContext local::GenerateMessageContext()
{
    MessageContext context(test::EndpointIdentifier, test::EndpointProtocol);

    context.BindEncryptionHandlers(
        [] (auto const& buffer, auto) -> Security::Encryptor::result_type 
            { return Security::Buffer(buffer.begin(), buffer.end()); },
        [] (auto const& buffer, auto) -> Security::Decryptor::result_type 
            { return Security::Buffer(buffer.begin(), buffer.end()); });

    context.BindSignatureHandlers(
        [] (auto&) -> Security::Signator::result_type 
            { return 0; },
        [] (auto const&) -> Security::Verifier::result_type 
            { return Security::VerificationStatus::Success; },
        [] () -> Security::SignatureSizeGetter::result_type 
            { return 0; });

    return context;
}

//----------------------------------------------------------------------------------------------------------------------
