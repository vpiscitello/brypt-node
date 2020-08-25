//------------------------------------------------------------------------------------------------
#include "../../BryptIdentifier/BryptIdentifier.hpp"
#include "../../BryptNode/BryptNode.hpp"
#include "../../Components/Command/Handler.hpp"
#include "../../Components/Endpoints/EndpointIdentifier.hpp"
#include "../../Components/Endpoints/TechnologyType.hpp"
#include "../../Components/MessageQueue/MessageQueue.hpp"
#include "../../Configuration/Configuration.hpp"
#include "../../Configuration/ConfigurationManager.hpp"
#include "../../Configuration/PeerPersistor.hpp"
#include "../../Message/Message.hpp"
#include "../../Message/MessageBuilder.hpp"
#include "../../Utilities/NodeUtils.hpp"
//------------------------------------------------------------------------------------------------
#include "../../Libraries/googletest/include/gtest/gtest.h"
//------------------------------------------------------------------------------------------------
#include <cstdint>
#include <chrono>
#include <set>
#include <string>
#include <string_view>
//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
namespace {
namespace local {
//------------------------------------------------------------------------------------------------

void SetupCommandHandlerMap(Command::HandlerMap& commands, CBryptNode& node);

//------------------------------------------------------------------------------------------------
} // local namespace
//------------------------------------------------------------------------------------------------
namespace test {
//------------------------------------------------------------------------------------------------

Configuration::TEndpointOptions CreateEndpointOptions();
std::unique_ptr<Configuration::CManager> CreateConfigurationManager();

BryptIdentifier::CContainer const ClientId(BryptIdentifier::Generate());
BryptIdentifier::CContainer const ServerId(BryptIdentifier::Generate());

constexpr std::string_view TechnologyName = "Direct";
constexpr Endpoints::TechnologyType TechnologyType = Endpoints::TechnologyType::Direct;
constexpr std::string_view Interface = "lo";
constexpr std::string_view ServerBinding = "*:35216";
constexpr std::string_view ClientBinding = "*:35217";
constexpr std::string_view ServerEntry = "127.0.0.1:35216";
constexpr std::string_view ClientEntry = "127.0.0.1:35217";

constexpr std::uint8_t BasePhase = 0;
constexpr std::string_view Message = "Hello World!";
constexpr std::uint32_t Nonce = 9999;

constexpr Endpoints::EndpointIdType const identifier = 1;
constexpr Endpoints::TechnologyType const technology = Endpoints::TechnologyType::TCP;
CMessageContext const context(identifier, technology);

//------------------------------------------------------------------------------------------------
} // local namespace
} // namespace
//------------------------------------------------------------------------------------------------

// TODO: Add tests for command sequences using connections

TEST(CommandSuite, CommandMatchingTest)
{
    auto const upConfigurationManager = test::CreateConfigurationManager();

    // The node itself will set up internal commands that can operate on it's
    // internal state, but in order to setup our own we need to provide the 
    // commands a node instance and a state.
    CBryptNode node(test::ServerId, nullptr, nullptr, nullptr, upConfigurationManager);
    Command::HandlerMap commands;
    local::SetupCommandHandlerMap(commands, node);

    OptionalMessage const optConnectRequest = CMessage::Builder()
        .SetMessageContext(test::context)
        .SetSource(test::ClientId)
        .SetDestination(test::ServerId)
        .SetCommand(Command::Type::Connect, test::BasePhase)
        .SetData(test::Message, test::Nonce)
        .ValidatedBuild();
    
    auto const connectCommandItr = commands.find(optConnectRequest->GetCommandType());
    ASSERT_NE(connectCommandItr, commands.end());

    auto const connectCommandReturnType = connectCommandItr->second->GetType();
    EXPECT_EQ(connectCommandReturnType, Command::Type::Connect);

    OptionalMessage const optElectionRequest = CMessage::Builder()
        .SetMessageContext(test::context)
        .SetSource(test::ClientId)
        .SetDestination(test::ServerId)
        .SetCommand(Command::Type::Election, test::BasePhase)
        .SetData(test::Message, test::Nonce)
        .ValidatedBuild();

    auto const electionCommandItr = commands.find(optElectionRequest->GetCommandType());
    ASSERT_NE(electionCommandItr, commands.end());

    auto const electionCommandReturnType = electionCommandItr->second->GetType();
    EXPECT_EQ(electionCommandReturnType, Command::Type::Election);

    OptionalMessage const optInformationRequest = CMessage::Builder()
        .SetMessageContext(test::context)
        .SetSource(test::ClientId)
        .SetDestination(test::ServerId)
        .SetCommand(Command::Type::Information, test::BasePhase)
        .SetData(test::Message, test::Nonce)
        .ValidatedBuild();
    
    auto const informationCommandItr = commands.find(optInformationRequest->GetCommandType());
    ASSERT_NE(informationCommandItr, commands.end());

    auto const informationCommandReturnType = informationCommandItr->second->GetType();
    EXPECT_EQ(informationCommandReturnType, Command::Type::Information);

    OptionalMessage const optQueryRequest = CMessage::Builder()
        .SetMessageContext(test::context)
        .SetSource(test::ClientId)
        .SetDestination(test::ServerId)
        .SetCommand(Command::Type::Query, test::BasePhase)
        .SetData(test::Message, test::Nonce)
        .ValidatedBuild();
    
    auto const queryCommandItr = commands.find(optQueryRequest->GetCommandType());
    ASSERT_NE(queryCommandItr, commands.end());

    auto const queryCommandReturnType = queryCommandItr->second->GetType();
    EXPECT_EQ(queryCommandReturnType, Command::Type::Query);

    OptionalMessage const optTransformRequest = CMessage::Builder()
        .SetMessageContext(test::context)
        .SetSource(test::ClientId)
        .SetDestination(test::ServerId)
        .SetCommand(Command::Type::Transform, test::BasePhase)
        .SetData(test::Message, test::Nonce)
        .ValidatedBuild();
    
    auto const transformCommandItr = commands.find(optTransformRequest->GetCommandType());
    ASSERT_NE(transformCommandItr, commands.end());

    auto const transformCommandReturnType = transformCommandItr->second->GetType();
    EXPECT_EQ(transformCommandReturnType, Command::Type::Transform);
}

//------------------------------------------------------------------------------------------------

Configuration::TEndpointOptions test::CreateEndpointOptions()
{
    Configuration::TEndpointOptions options(
        test::TechnologyType,
        test::Interface,
        test::ServerBinding);

    return options;
}

//------------------------------------------------------------------------------------------------

std::unique_ptr<Configuration::CManager> test::CreateConfigurationManager()
{
    auto const endpointOptions = CreateEndpointOptions();
    Configuration::TSettings settings(
        Configuration::TDetailsOptions("test-node"),
        {endpointOptions},
        Configuration::TSecurityOptions()
    );

    return std::make_unique<Configuration::CManager>(settings);
}

//------------------------------------------------------------------------------------------------

void local::SetupCommandHandlerMap(
    Command::HandlerMap& commands, CBryptNode& node)
{
    commands.emplace(
        Command::Type::Connect,
        Command::Factory(Command::Type::Connect, node));

    commands.emplace(
        Command::Type::Election,
        Command::Factory(Command::Type::Election, node));

    commands.emplace(
        Command::Type::Information,
        Command::Factory(Command::Type::Information, node));
  
    commands.emplace(
        Command::Type::Query,
        Command::Factory(Command::Type::Query, node));

    commands.emplace(
        Command::Type::Transform,
        Command::Factory(Command::Type::Transform, node));
}

//------------------------------------------------------------------------------------------------