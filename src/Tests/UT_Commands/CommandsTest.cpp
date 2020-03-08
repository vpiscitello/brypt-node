//------------------------------------------------------------------------------------------------
#include "../../Node.hpp"
#include "../../State.hpp"
#include "../../Components/Command/Handler.hpp"
#include "../../Configuration/Configuration.hpp"
#include "../../Utilities/Message.hpp"
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

void SetupCommandMap(
    NodeUtils::CommandMap& commands, CNode& node, std::weak_ptr<CState> state);

//------------------------------------------------------------------------------------------------
} // local namespace
//------------------------------------------------------------------------------------------------
namespace test {
//------------------------------------------------------------------------------------------------

Configuration::TConnectionOptions CreateConnectionOptions();
Configuration::TSettings CreateConfigurationSettings();

constexpr NodeUtils::NodeIdType ServerId = 0x12345678;
constexpr NodeUtils::NodeIdType ClientId = 0xFFFFFFFF;
constexpr std::string_view TechnologyName = "Direct";
constexpr NodeUtils::TechnologyType TechnologyType = NodeUtils::TechnologyType::Direct;
constexpr std::string_view Interface = "lo";
constexpr std::string_view ServerBinding = "*:3000";
constexpr std::string_view ClientBinding = "*:3001";
constexpr std::string_view ServerEntry = "127.0.0.1:3000";
constexpr std::string_view ClientEntry = "127.0.0.1:3001";

constexpr std::uint8_t BasePhase = 0;
constexpr std::string_view Message = "Hello World!";
constexpr std::uint32_t Nonce = 9999;

//------------------------------------------------------------------------------------------------
} // local namespace
} // namespace
//------------------------------------------------------------------------------------------------

// TODO: Add tests for command sequences using connections

TEST(CommandSuite, CommandMatchingTest)
{
    auto const settings = test::CreateConfigurationSettings();

    // The node itself will set up internal commands that can operate on it's
    // internal state, but in order to setup our own we need to provide the 
    // commands a node instance and a state.
    CNode node(settings);
    std::shared_ptr<CState> state = std::make_shared<CState>(settings);
    NodeUtils::CommandMap commands;
    local::SetupCommandMap(commands, node, state);

    CMessage const connectRequest(
        test::ServerId, test::ClientId,
        NodeUtils::CommandType::Connect, test::BasePhase,
        test::Message, test::Nonce);
    
    auto const connectCommandItr = commands.find(connectRequest.GetCommand());
    ASSERT_NE(connectCommandItr, commands.end());

    auto const connectCommandReturnType = connectCommandItr->second->GetType();
    EXPECT_EQ(connectCommandReturnType, NodeUtils::CommandType::Connect);

    CMessage const electionRequest(
        test::ServerId, test::ClientId,
        NodeUtils::CommandType::Election, test::BasePhase,
        test::Message, test::Nonce);
    
    auto const electionCommandItr = commands.find(electionRequest.GetCommand());
    ASSERT_NE(electionCommandItr, commands.end());

    auto const electionCommandReturnType = electionCommandItr->second->GetType();
    EXPECT_EQ(electionCommandReturnType, NodeUtils::CommandType::Election);

    CMessage const informationRequest(
        test::ServerId, test::ClientId,
        NodeUtils::CommandType::Information, test::BasePhase,
        test::Message, test::Nonce);
    
    auto const informationCommandItr = commands.find(informationRequest.GetCommand());
    ASSERT_NE(informationCommandItr, commands.end());

    auto const informationCommandReturnType = informationCommandItr->second->GetType();
    EXPECT_EQ(informationCommandReturnType, NodeUtils::CommandType::Information);

    CMessage const queryRequest(
        test::ServerId, test::ClientId,
        NodeUtils::CommandType::Query, test::BasePhase,
        test::Message, test::Nonce);
    
    auto const queryCommandItr = commands.find(queryRequest.GetCommand());
    ASSERT_NE(queryCommandItr, commands.end());

    auto const queryCommandReturnType = queryCommandItr->second->GetType();
    EXPECT_EQ(queryCommandReturnType, NodeUtils::CommandType::Query);

    CMessage const transformRequest(
        test::ServerId, test::ClientId,
        NodeUtils::CommandType::Transform, test::BasePhase,
        test::Message, test::Nonce);
    
    auto const transformCommandItr = commands.find(transformRequest.GetCommand());
    ASSERT_NE(transformCommandItr, commands.end());

    auto const transformCommandReturnType = transformCommandItr->second->GetType();
    EXPECT_EQ(transformCommandReturnType, NodeUtils::CommandType::Transform);
}

//------------------------------------------------------------------------------------------------

Configuration::TConnectionOptions test::CreateConnectionOptions()
{
    Configuration::TConnectionOptions options(
        test::ClientId,
        test::TechnologyType,
        test::Interface,
        test::ServerBinding);

    options.operation = NodeUtils::ConnectionOperation::Server;
    
    return options;
}

//------------------------------------------------------------------------------------------------

Configuration::TSettings test::CreateConfigurationSettings()
{
    auto const connectionOptions = CreateConnectionOptions();
    Configuration::TSettings settings(
        Configuration::TDetailsOptions("test-node"),
        {connectionOptions},
        Configuration::TSecurityOptions()
    );

    return settings;
}

//------------------------------------------------------------------------------------------------

void local::SetupCommandMap(
    NodeUtils::CommandMap& commands, CNode& node, std::weak_ptr<CState> state)
{
    commands.emplace(
        NodeUtils::CommandType::Connect,
        Command::Factory(NodeUtils::CommandType::Connect, node, state));

    commands.emplace(
        NodeUtils::CommandType::Election,
        Command::Factory(NodeUtils::CommandType::Election, node, state));

    commands.emplace(
        NodeUtils::CommandType::Information,
        Command::Factory(NodeUtils::CommandType::Information, node, state));
  
    commands.emplace(
        NodeUtils::CommandType::Query,
        Command::Factory(NodeUtils::CommandType::Query, node, state));

    commands.emplace(
        NodeUtils::CommandType::Transform,
        Command::Factory(NodeUtils::CommandType::Transform, node, state));
}

//------------------------------------------------------------------------------------------------