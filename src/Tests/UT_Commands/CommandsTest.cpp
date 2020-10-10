//------------------------------------------------------------------------------------------------
#include "../../BryptIdentifier/BryptIdentifier.hpp"
#include "../../BryptNode/BryptNode.hpp"
#include "../../Components/Command/Handler.hpp"
#include "../../Components/Endpoints/EndpointIdentifier.hpp"
#include "../../Components/Endpoints/TechnologyType.hpp"
#include "../../Configuration/Configuration.hpp"
#include "../../Configuration/ConfigurationManager.hpp"
#include "../../Configuration/PeerPersistor.hpp"
#include "../../BryptMessage/ApplicationMessage.hpp"
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

BryptIdentifier::CContainer const ClientIdentifier(BryptIdentifier::Generate());
auto const spServerIdentifier = std::make_shared<BryptIdentifier::CContainer const>(
    BryptIdentifier::Generate());

constexpr std::string_view TechnologyName = "TCP";
constexpr Endpoints::TechnologyType TechnologyType = Endpoints::TechnologyType::TCP;
constexpr std::string_view Interface = "lo";
constexpr std::string_view ServerBinding = "*:35216";
constexpr std::string_view ClientBinding = "*:35217";
constexpr std::string_view ServerEntry = "127.0.0.1:35216";
constexpr std::string_view ClientEntry = "127.0.0.1:35217";

constexpr std::uint8_t BasePhase = 0;
constexpr std::string_view Message = "Hello World!";

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
    CBryptNode node(
        test::spServerIdentifier, nullptr, nullptr, nullptr, nullptr, upConfigurationManager);
    Command::HandlerMap commands;
    local::SetupCommandHandlerMap(commands, node);

    auto const optConnectRequest = CApplicationMessage::Builder()
        .SetMessageContext(test::context)
        .SetSource(test::ClientIdentifier)
        .SetDestination(*test::spServerIdentifier)
        .SetCommand(Command::Type::Connect, test::BasePhase)
        .SetData(test::Message)
        .ValidatedBuild();
    
    auto const connectCommandItr = commands.find(optConnectRequest->GetCommand());
    ASSERT_NE(connectCommandItr, commands.end());

    auto const connectCommandReturnType = connectCommandItr->second->GetType();
    EXPECT_EQ(connectCommandReturnType, Command::Type::Connect);

    auto const optElectionRequest = CApplicationMessage::Builder()
        .SetMessageContext(test::context)
        .SetSource(test::ClientIdentifier)
        .SetDestination(*test::spServerIdentifier)
        .SetCommand(Command::Type::Election, test::BasePhase)
        .SetData(test::Message)
        .ValidatedBuild();

    auto const electionCommandItr = commands.find(optElectionRequest->GetCommand());
    ASSERT_NE(electionCommandItr, commands.end());

    auto const electionCommandReturnType = electionCommandItr->second->GetType();
    EXPECT_EQ(electionCommandReturnType, Command::Type::Election);

    auto const optInformationRequest = CApplicationMessage::Builder()
        .SetMessageContext(test::context)
        .SetSource(test::ClientIdentifier)
        .SetDestination(*test::spServerIdentifier)
        .SetCommand(Command::Type::Information, test::BasePhase)
        .SetData(test::Message)
        .ValidatedBuild();
    
    auto const informationCommandItr = commands.find(optInformationRequest->GetCommand());
    ASSERT_NE(informationCommandItr, commands.end());

    auto const informationCommandReturnType = informationCommandItr->second->GetType();
    EXPECT_EQ(informationCommandReturnType, Command::Type::Information);

    auto const optQueryRequest = CApplicationMessage::Builder()
        .SetMessageContext(test::context)
        .SetSource(test::ClientIdentifier)
        .SetDestination(*test::spServerIdentifier)
        .SetCommand(Command::Type::Query, test::BasePhase)
        .SetData(test::Message)
        .ValidatedBuild();
    
    auto const queryCommandItr = commands.find(optQueryRequest->GetCommand());
    ASSERT_NE(queryCommandItr, commands.end());

    auto const queryCommandReturnType = queryCommandItr->second->GetType();
    EXPECT_EQ(queryCommandReturnType, Command::Type::Query);
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

}

//------------------------------------------------------------------------------------------------