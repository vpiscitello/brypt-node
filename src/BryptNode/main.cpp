//----------------------------------------------------------------------------------------------------------------------
// File: main.cpp
// Description:
//----------------------------------------------------------------------------------------------------------------------
#include "BryptNode.hpp"
#include "RuntimeContext.hpp"
#include "StartupOptions.hpp"
#include "BryptIdentifier/BryptIdentifier.hpp"
#include "Components/Configuration/Configuration.hpp"
#include "Components/Configuration/Parser.hpp"
#include "Components/Configuration/BootstrapService.hpp"
#include "Components/Event/Publisher.hpp"
#include "Components/MessageControl/AuthorizedProcessor.hpp"
#include "Components/MessageControl/DiscoveryProtocol.hpp"
#include "Components/Network/EndpointTypes.hpp"
#include "Components/Network/Manager.hpp"
#include "Components/Peer/Manager.hpp"
#include "Utilities/Assertions.hpp"
#include "Utilities/ExecutionResult.hpp"
#include "Utilities/LogUtils.hpp"
#include "Utilities/Version.hpp"
//----------------------------------------------------------------------------------------------------------------------
#include <cassert>
#include <cstdint>
//----------------------------------------------------------------------------------------------------------------------

std::int32_t main(std::int32_t argc, char** argv)
{
    Startup::Options options;
    if (auto const status = options.Parse(argc, argv); status != Startup::ParseCode::Success) {
        if (status == Startup::ParseCode::ExitRequested) { return 0; }
        else { std::cout << "Unable to parse startup options!" << std::endl; return 1; }
    }
    
    assert(Assertions::Threading::IsCoreThread()); // Note: We must ensure the main thread id is set.
    LogUtils::InitializeLoggers(options.GetVerbosityLevel()); // Note: We must ensure the loggers are initialized.
    auto const spLogger = spdlog::get(LogUtils::Name::Core.data());  

    auto const upParser = std::make_unique<Configuration::Parser>(options.GetConfigPath(), options.IsInteractive());
    if (auto const status = upParser->FetchSettings(); status != Configuration::StatusCode::Success) {
        spLogger->critical("An unexpected error occured while parsing the configuration file!");
        return 1;
    }

    auto const spBootstrapService = std::make_shared<BootstrapService>(
        options.GetBootstrapPath(), upParser->GetEndpointOptions());
    if (!spBootstrapService->FetchBootstraps()) {
        spLogger->critical("An error occured parsing bootstraps!");
        return 1;
    }

    auto const spPublisher = std::make_shared<Event::Publisher>();
    auto const spProtocol = std::make_shared<DiscoveryProtocol>(upParser->GetEndpointOptions());
    auto const spProcessor = std::make_shared<AuthorizedProcessor>(upParser->GetNodeIdentifier());
    auto const spPeerManager = std::make_shared<Peer::Manager>(
        upParser->GetNodeIdentifier(), upParser->GetSecurityStrategy(), spProtocol, spProcessor);

    spBootstrapService->SetMediator(spPeerManager.get());

    IBootstrapCache const* const pBootstraps = (options.UseBootstraps()) ? spBootstrapService.get() : nullptr;
    auto const spNetworkManager = std::make_shared<Network::Manager>(
        upParser->GetEndpointOptions(), spPublisher, spPeerManager.get(), pBootstraps, RuntimeContext::Foreground);

    BryptNode alpha(upParser, spPublisher, spNetworkManager, spPeerManager, spProcessor, spBootstrapService);
    assert(alpha.IsInitialized());

    spLogger->info("Welcome to the Brypt Network!");
    spLogger->info("Brypt Identifier: {}", upParser->GetNodeIdentifier());

    ExecutionResult const result = alpha.Startup();
    if (result == ExecutionResult::UnexpectedShutdown) {
        spLogger->critical("An unexpected error caused the node to shutdown!");
        return 1;
    }
    // Currently, RequestedShutdown and UnexpectedShutdown are the only values that should be returned in the 
    // foreground context. If we get InitializationFailed, AlreadyStarted, or ThreadSpawned something went wrong 
    assert(result == ExecutionResult::RequestedShutdown);

    return 0;
}

//----------------------------------------------------------------------------------------------------------------------
