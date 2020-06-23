//------------------------------------------------------------------------------------------------
// File: Test.cpp
// Description:
//------------------------------------------------------------------------------------------------
#include "BryptNode.hpp"
#include "../Components/Endpoints/EndpointTypes.hpp"
#include "../Components/Endpoints/EndpointManager.hpp"
#include "../Components/MessageQueue/MessageQueue.hpp"
#include "../Configuration/Configuration.hpp"
#include "../Configuration/ConfigurationManager.hpp"
#include "../Configuration/PeerPersistor.hpp"
#include "../Utilities/Message.hpp"
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

std::string ConfigurationFilename = "";
void ParseArguments(std::int32_t argc, char** argv);

//------------------------------------------------------------------------------------------------
} // local namespace
} // namespace
//------------------------------------------------------------------------------------------------


//------------------------------------------------------------------------------------------------

std::int32_t main(std::int32_t argc, char** argv)
{
    std::cout << "\n== Welcome to the Brypt Network\n";
    std::cout << "Main process PID: " << getpid() << "\n";

    local::ParseArguments(argc, argv);

    std::unique_ptr<Configuration::CManager> upConfigurationManager;
    if (local::ConfigurationFilename.empty()) {
        upConfigurationManager = std::make_unique<Configuration::CManager>();
    } else {
        upConfigurationManager = std::make_unique<Configuration::CManager>(local::ConfigurationFilename);
    }

    auto optSettings = upConfigurationManager->FetchSettings();
    if (!optSettings) {
        std::cout << "Node configuration settings could not be parsed!" << std::endl;
        exit(1);
    }
    
    auto spPersistor = std::make_shared<CPeerPersistor>();
    if (!spPersistor->FetchPeers()) {
        std::cout << "Node bootstraps could not be parsed!" << std::endl;
        exit(1);
    }

    auto spMessageQueue = std::make_shared<CMessageQueue>();
    auto spEndpointManager = std::make_shared<CEndpointManager>();
    Configuration::EndpointConfigurations const& configurations = optSettings->endpoints;

    assert(spEndpointManager);
    assert(spMessageQueue);
    assert(spPersistor);
    spEndpointManager->Initialize(
        0,   // TODO: Get Brypt Node ID and pass it in to the manager
        spMessageQueue.get(),
        configurations,
        spPersistor.get());

    spPersistor->SetMediator(spEndpointManager.get());

    if (!optSettings->endpoints.empty()) {
        CBryptNode alpha(spEndpointManager, spMessageQueue, spPersistor, *optSettings);
        alpha.Startup();
    }

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
}
//------------------------------------------------------------------------------------------------
