//------------------------------------------------------------------------------------------------
// File: main.cpp
// Description:
//------------------------------------------------------------------------------------------------
#include "BryptNode.hpp"
#include "BryptIdentifier/BryptIdentifier.hpp"
#include "Components/BryptPeer/PeerManager.hpp"
#include "Components/Configuration/Configuration.hpp"
#include "Components/Configuration/ConfigurationManager.hpp"
#include "Components/Configuration/PeerPersistor.hpp"
#include "Components/MessageControl/AuthorizedProcessor.hpp"
#include "Components/MessageControl/DiscoveryProtocol.hpp"
#include "Components/Network/EndpointTypes.hpp"
#include "Components/Network/EndpointManager.hpp"
#include "Utilities/LogUtils.hpp"
//------------------------------------------------------------------------------------------------
#include <cstdint>
#include <optional>
#include <string>
//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
namespace {
namespace local {
//------------------------------------------------------------------------------------------------

std::string PeersFilename = "";
std::string ConfigurationFilename = "";
void ParseArguments(std::int32_t argc, char** argv);

//------------------------------------------------------------------------------------------------
} // local namespace
} // namespace
//------------------------------------------------------------------------------------------------

std::int32_t main(std::int32_t argc, char** argv)
{
    local::ParseArguments(argc, argv);
    LogUtils::InitializeLoggers();

    auto const spCoreLogger = spdlog::get(LogUtils::Name::Core.data());  
    spCoreLogger->set_level(spdlog::level::debug);
    spCoreLogger->info("Welcome to the Brypt Network!");

    std::unique_ptr<Configuration::Manager> upConfigurationManager;
    if (local::ConfigurationFilename.empty()) {
        upConfigurationManager = std::make_unique<Configuration::Manager>();
    } else {
        upConfigurationManager = std::make_unique<Configuration::Manager>(
            local::ConfigurationFilename);
    }

    auto const status = upConfigurationManager->FetchSettings();
    if (status != Configuration::StatusCode::Success) {
        spCoreLogger->critical("Error occured parsing settings!");
        exit(1);
    }

    auto const spBryptIdentifier = upConfigurationManager->GetBryptIdentifier();
    if (!spBryptIdentifier) {
        spCoreLogger->critical("Error occured establishing a Brypt Identifier!");
        exit(1);
    }

    spCoreLogger->info(
        "Brypt Identifier: {}", spBryptIdentifier->GetNetworkRepresentation());

    auto const optEndpointConfigurations = upConfigurationManager->GetEndpointConfigurations();
    if (!optEndpointConfigurations) {
        spCoreLogger->critical("Error occured parsing endpoint configurations!");
        exit(1);
    }

    auto const spPeerPersistor = std::make_shared<PeerPersistor>(
        local::PeersFilename, *optEndpointConfigurations);

    if (!spPeerPersistor->FetchBootstraps()) {
        spCoreLogger->critical("Error occured parsing bootstraps!");
        exit(1);
    }

    auto const spDiscoveryProtocol = std::make_shared<DiscoveryProtocol>(
        *optEndpointConfigurations);

    auto const spMessageCollector = std::make_shared<AuthorizedProcessor>();

    auto const spPeerManager = std::make_shared<PeerManager>(
        spBryptIdentifier, upConfigurationManager->GetSecurityStrategy(),
        spDiscoveryProtocol, spMessageCollector);

    spPeerPersistor->SetMediator(spPeerManager.get());

    auto const spEndpointManager = std::make_shared<EndpointManager>(
        *optEndpointConfigurations, spPeerManager.get(), spPeerPersistor.get());

    BryptNode alpha(
        spBryptIdentifier, spEndpointManager, spPeerManager, 
        spMessageCollector, spPeerPersistor, upConfigurationManager);

    alpha.Startup();

    return 0;
}

//------------------------------------------------------------------------------------------------

void local::ParseArguments(std::int32_t argc, char** argv)
{
    std::vector<std::string> arguments;
    std::vector<std::string>::iterator itr;

    for (std::int32_t idx = 0; idx < argc; ++idx) {
        if (auto const& argument = argv[idx]; argument != nullptr) {
        arguments.push_back(std::string(argument));
        }
    }

    itr = find (arguments.begin(), arguments.end(), "--config");
    if (itr != arguments.end()) {
        local::ConfigurationFilename = *(++itr);
    }

    itr = find (arguments.begin(), arguments.end(), "--peers");
    if (itr != arguments.end()) {
        local::PeersFilename = *(++itr);
    }
}
//------------------------------------------------------------------------------------------------
