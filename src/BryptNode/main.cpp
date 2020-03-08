//------------------------------------------------------------------------------------------------
// File: Test.cpp
// Description:
//------------------------------------------------------------------------------------------------
#include "BryptNode.hpp"
#include "../Configuration/Configuration.hpp"
#include "../Configuration/ConfigurationManager.hpp"
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
//------------------------------------------------------------------------------------------------
namespace local {
//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
} // local namespace
//------------------------------------------------------------------------------------------------
} // namespace
//------------------------------------------------------------------------------------------------

Configuration::TConnectionOptions options;

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
    std::optional<Configuration::TSettings> optSettings = configurationManager.Parse();
    if (optSettings) {
        Configuration::TSettings& settings = *optSettings;
        ParseArguments(argc, argv, settings);
        if (!settings.connections.empty()) {
            if (options.operation != NodeUtils::ConnectionOperation::None) {
                settings.connections[0].operation = options.operation;
                
                CBryptNode alpha(*optSettings);
                alpha.Startup();
            } else {
                std::cout << "Node Setup must be provided and device operation type!" << std::endl;
                exit(-1);
            }
        }
    } else {
        std::cout << "Node configuration settings could not be parsed!" << std::endl;
        exit(1);
    }

    return 0;
}

//------------------------------------------------------------------------------------------------