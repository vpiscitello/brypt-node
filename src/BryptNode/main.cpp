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
#include <fcntl.h>
#include <unistd.h>
#include <cstdint>
#include <climits>
#include <cstdint>
#include <fstream>
#include <chrono>
#include <memory>
#include <optional>
#include <string>
#include <thread>
#include <sys/stat.h>
#include <sys/types.h>
//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
namespace {
namespace local {
//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
} // local namespace
} // namespace
//------------------------------------------------------------------------------------------------

Configuration::TEndpointOptions options;

//------------------------------------------------------------------------------------------------

void ParseArguments(
    std::int32_t argc,
    char** argv,
    Configuration::TSettings &settings)
{
    std::vector<std::string> args;
    std::vector<std::string>::iterator itr;

    if (argc <= 1) {
        NodeUtils::printo("Arguments must be provided!", NodeUtils::PrintType::Error);
        exit(1);
    }

    for (std::int32_t idx = 0; idx < argc; ++idx) {
        args.push_back(std::string(argv[idx]));
    }

    static std::unordered_map<std::string, NodeUtils::DeviceOperation> deviceOperationMap = 
    {
        {"--root", NodeUtils::DeviceOperation::Root},
        {"--branch", NodeUtils::DeviceOperation::Branch},
        {"--leaf", NodeUtils::DeviceOperation::Leaf}
    };

    for (auto const [key, value] : deviceOperationMap) {
        if (itr = std::find(args.begin(), args.end(), key); itr != args.end()) {
            settings.details.operation = value;
            break;
        }
    }
}
//------------------------------------------------------------------------------------------------

std::int32_t main(std::int32_t argc, char** argv)
{
    std::cout << "\n== Welcome to the Brypt Network\n";
    std::cout << "Main process PID: " << getpid() << "\n";

    Configuration::CManager configurationManager;
    auto optSettings = configurationManager.FetchSettings();
    if (!optSettings) {
        std::cout << "Node configuration settings could not be parsed!" << std::endl;
        exit(1);
    }
    
    
    Configuration::CPeerPersistor persistor;
    auto optBootstraps = persistor.FetchPeers();
    if (!optBootstraps) {
        std::cout << "Node bootstraps could not be parsed!" << std::endl;
        exit(1);
    }

    auto spMessageQueue = std::make_shared<CMessageQueue>();
    auto spEndpointManager = std::make_shared<CEndpointManager>();
    Configuration::EndpointConfigurations const& configurations = optSettings->endpoints;
    spEndpointManager->Initialize(
        0,   // TODO: Get Brypt Node ID and pass it in to the manager
        spMessageQueue.get(),
        configurations,
        optBootstraps);

    persistor.SetMediator(spEndpointManager.get());

    ParseArguments(argc, argv, *optSettings);
    if (!optSettings->endpoints.empty()) {
        CBryptNode alpha(spEndpointManager, spMessageQueue, *optSettings);
        alpha.Startup();
    }

    return 0;
}

//------------------------------------------------------------------------------------------------
