//----------------------------------------------------------------------------------------------------------------------
// File: main.cpp
// Description:
//----------------------------------------------------------------------------------------------------------------------
#include "BryptNode.hpp"
#include "ExecutionToken.hpp"
#include "RuntimeContext.hpp"
#include "StartupOptions.hpp"
#include "BryptIdentifier/BryptIdentifier.hpp"
#include "Components/Configuration/Options.hpp"
#include "Components/Configuration/Parser.hpp"
#include "Components/Configuration/BootstrapService.hpp"
#include "Components/Event/Publisher.hpp"
#include "Components/MessageControl/AuthorizedProcessor.hpp"
#include "Components/Network/EndpointTypes.hpp"
#include "Components/Network/Manager.hpp"
#include "Components/Peer/Manager.hpp"
#include "Utilities/Assertions.hpp"
#include "Utilities/ExecutionStatus.hpp"
#include "Utilities/LogUtils.hpp"
#include "Utilities/Version.hpp"
//----------------------------------------------------------------------------------------------------------------------
#include <boost/core/quick_exit.hpp>
//----------------------------------------------------------------------------------------------------------------------
#include <cassert>
#include <csignal>
#include <cstdint>
//----------------------------------------------------------------------------------------------------------------------

//----------------------------------------------------------------------------------------------------------------------
namespace {
namespace Signal {
//----------------------------------------------------------------------------------------------------------------------

Node::ExecutionToken ExecutionToken;

extern "C" void OnShutdownRequested(std::int32_t signal);

//----------------------------------------------------------------------------------------------------------------------
} // Signal namespace
} // namespace
//----------------------------------------------------------------------------------------------------------------------

//----------------------------------------------------------------------------------------------------------------------
namespace Startup {
//----------------------------------------------------------------------------------------------------------------------

using Resources = std::tuple<ParseCode, std::unique_ptr<Configuration::Parser>, std::shared_ptr<BootstrapService>>;

Resources InitializeResources(std::int32_t argc, char** argv);

//----------------------------------------------------------------------------------------------------------------------
} // Startup namespace
//----------------------------------------------------------------------------------------------------------------------

extern "C" void Signal::OnShutdownRequested(std::int32_t signal)
{
    // Note: std::quick_exit() is for some reason still unimplemented on MacOS, boost::quick_exit will alias to the 
    // next best alternative (e.g. __Exit()). We could use abort(), but we would loose the conext of the caught signal.
    // If we haven't started the node yet, there is no additional cleanup required (std::exit() is not signal safe).
    if (!ExecutionToken.IsExecutionActive()) { boost::quick_exit(signal); }
    // Use the token to notify the node that a shutdown has been requested. In the context of the console application,
    // SIGINT and SIGTERM are considered are expected shutdown signals (i.e. not errors).
    [[maybe_unused]] bool const result = ExecutionToken.RequestStop();
}

//----------------------------------------------------------------------------------------------------------------------

std::int32_t main(std::int32_t argc, char** argv)
{
    // Set the core thread such that any non-direct core resources can assert during initialization. 
    assert(Assertions::Threading::SetCoreThread());

    // Register listeners such that we can properly handle shutdown requests via process signals. 
    if (SIG_ERR == std::signal(SIGINT, Signal::OnShutdownRequested)) { return 1; }
    if (SIG_ERR == std::signal(SIGTERM, Signal::OnShutdownRequested)) { return 1; }

    auto const [code, upParser, spBootstrapService] = Startup::InitializeResources(argc, argv);
    switch (code) {
        case Startup::ParseCode::Success: assert(upParser && spBootstrapService); break;
        case Startup::ParseCode::ExitRequested: return 0;
        case Startup::ParseCode::Malformed: return 1;
    }

    auto const logger = spdlog::get(LogUtils::Name::Core.data());  
    logger->info("Welcome to the Brypt Network!");
    logger->info("Brypt Identifier: {}", upParser->GetNodeIdentifier());

    Node::Core core(Signal::ExecutionToken, upParser, spBootstrapService);

    auto const status = core.Startup(); // Start the core, the runtime will block will until execution completes.
    switch (status) {
        case ExecutionStatus::RequestedShutdown: break;
        // Log an error and return a non-success code if the core fails to begin execution due to an initialization error. 
        case ExecutionStatus::InitializationFailed: {
            logger->critical("Failed to initialize core resources!");
            return 1;
        }
        // Log an error and return a non-success code if the runtime is shutdown due to an unexpected error. 
        case ExecutionStatus::UnexpectedShutdown: {
            logger->critical("An unexpected error caused the node to shutdown!");
            return 1;
        }
         // Currently, only the two explicit status cases are expected for a foreground process. 
        default: assert(false); return 1;
    }

    return 0;
}

//----------------------------------------------------------------------------------------------------------------------

Startup::Resources Startup::InitializeResources(std::int32_t argc, char** argv)
{
    Options options; 
    switch (options.Parse(argc, argv)) {
        // On success, we can continue directly to initializing the runtime resources. 
        case ParseCode::Success: break;
        // Early return when we don't need initialize the resources (e.g. when "--help" is used).
        case ParseCode::ExitRequested: return { ParseCode::ExitRequested, nullptr, nullptr }; 
        // Log out an errur and early return when the provided flags were malformed. 
        default: {
            std::cout << "Unable to parse startup options!" << std::endl;
            return { ParseCode::Malformed, nullptr, nullptr }; 
        }
    }
    
    // Initialize the logging resources for the application. 
    LogUtils::InitializeLoggers(options.GetVerbosityLevel()); 
    auto const logger = spdlog::get(LogUtils::Name::Core.data()); // From here on we should use the logger for errors. 

    // Create a configuration parser to read the configuration file at the provided location. If we fail to read the
    // file log an error and return early. 
    auto upParser = std::make_unique<Configuration::Parser>(options.GetConfigPath(), options);
    if (auto const status = upParser->FetchOptions(); status != Configuration::StatusCode::Success) {
        logger->critical("An unexpected error occured while parsing the configuration file!");
        return { ParseCode::Malformed, nullptr, nullptr };
    }

    // Create the bootstrap service and read the initial bootstraps from the file. If we fail to fetch the bootstraps,
    // log an error and return early. 
    auto spBootstrapService = std::make_shared<BootstrapService>(
        options.GetBootstrapPath(), upParser->GetEndpointOptions(), upParser->UseFilepathDeduction());
    if (!spBootstrapService->FetchBootstraps()) {
        logger->critical("An error occured parsing bootstraps!");
        return { ParseCode::Malformed, nullptr, nullptr };
    }

    // Indicate that the startup resources have been successfully initialized and provide them to the caller. 
    return { ParseCode::Success, std::move(upParser), std::move(spBootstrapService) };
}

//----------------------------------------------------------------------------------------------------------------------
