//------------------------------------------------------------------------------------------------
// File: Test.cpp
// Description:
//------------------------------------------------------------------------------------------------
#include "BryptNode.hpp"
#include "../BryptIdentifier/BryptIdentifier.hpp"
#include "../Components/Endpoints/EndpointTypes.hpp"
#include "../Components/Endpoints/EndpointManager.hpp"
#include "../Components/MessageQueue/MessageQueue.hpp"
#include "../Configuration/Configuration.hpp"
#include "../Configuration/ConfigurationManager.hpp"
#include "../Configuration/PeerPersistor.hpp"
#include "../Utilities/NodeUtils.hpp"
//------------------------------------------------------------------------------------------------
#include "../Libraries/spdlog/spdlog.h"
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
    std::cout << std::endl;
    std::cout << "== Welcome to the Brypt Network" << std::endl;

    local::ParseArguments(argc, argv);

    std::unique_ptr<Configuration::CManager> upConfigurationManager;
    if (local::ConfigurationFilename.empty()) {
        upConfigurationManager = std::make_unique<Configuration::CManager>();
    } else {
        upConfigurationManager = std::make_unique<Configuration::CManager>(
            local::ConfigurationFilename);
    }

    auto const status = upConfigurationManager->FetchSettings();
    if (status != Configuration::StatusCode::Success) {
        std::cout << "Node configuration settings could not be parsed!" << std::endl;
        exit(1);
    }

    auto const optBryptIdentifier = upConfigurationManager->GetBryptIdentifier();
    if (!optBryptIdentifier) {
        std::cout << "A Brypt Identifier could not be established based on the specified"\
                     "configuration options!" << std::endl;
        exit(1);
    }

    std::cout << "== Brypt Identifier: " << *optBryptIdentifier << std::endl;

    auto const optEndpointConfigurations = upConfigurationManager->GetEndpointConfigurations();
    if (!optEndpointConfigurations) {
        std::cout << "No endpoint configurations could were parsed from the configuration"\
                     "file!" << std::endl;
        exit(1);
    }

    auto spEndpointManager = std::make_shared<CEndpointManager>();
    auto spPersistor = std::make_shared<CPeerPersistor>(
        local::PeersFilename,
        *optEndpointConfigurations);
    auto spMessageQueue = std::make_shared<CMessageQueue>();

    if (!spPersistor->FetchPeers()) {
        std::cout << "Node bootstraps could not be parsed!" << std::endl;
        exit(1);
    }

    spEndpointManager->Initialize(
        *optBryptIdentifier,
        spMessageQueue.get(),
        *optEndpointConfigurations,
        spPersistor.get());

    spPersistor->SetMediator(spEndpointManager.get());

    CBryptNode alpha(
        *optBryptIdentifier,
        spEndpointManager,
        spMessageQueue,
        spPersistor,
        upConfigurationManager);

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
