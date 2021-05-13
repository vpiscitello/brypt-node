//----------------------------------------------------------------------------------------------------------------------
// File: main.cpp
// Description:
//----------------------------------------------------------------------------------------------------------------------
#include "BryptNode.hpp"
#include "StartupOptions.hpp"
#include "BryptIdentifier/BryptIdentifier.hpp"
#include "Components/Configuration/Configuration.hpp"
#include "Components/Configuration/Manager.hpp"
#include "Components/Configuration/PeerPersistor.hpp"
#include "Components/Event/Publisher.hpp"
#include "Components/MessageControl/AuthorizedProcessor.hpp"
#include "Components/MessageControl/DiscoveryProtocol.hpp"
#include "Components/Network/EndpointTypes.hpp"
#include "Components/Network/Manager.hpp"
#include "Components/Peer/Manager.hpp"
#include "Utilities/ExecutionResult.hpp"
#include "Utilities/LogUtils.hpp"
#include "Utilities/Version.hpp"
//----------------------------------------------------------------------------------------------------------------------
#include <cstdint>
//----------------------------------------------------------------------------------------------------------------------

std::int32_t main(std::int32_t argc, char** argv)
{
    Startup::Options options;
    if (auto const status = options.Parse(argc, argv); status != Startup::ParseCode::Success) {
        if (status == Startup::ParseCode::ExitRequested) { return 0; }
        else { std::cout << "Unable to parse startup options!" << std::endl; return 1; }
    }

    LogUtils::InitializeLoggers(options.GetVerbosityLevel());
    auto const spLogger = spdlog::get(LogUtils::Name::Core.data());  

    auto const upConfig = std::make_unique<Configuration::Manager>(options.GetConfigPath(), options.IsInteractive());
    if (auto const status = upConfig->FetchSettings(); status != Configuration::StatusCode::Success) {
        spLogger->critical("An unexpected error occured while parsing the configuration file!");
        return 1;
    }

    auto const spPersistor = std::make_shared<PeerPersistor>(options.GetPeersPath(), upConfig->GetEndpointOptions());
    if (!spPersistor->FetchBootstraps()) {
        spLogger->critical("An error occured parsing bootstraps!");
        return 1;
    }

    auto const spEventPublisher = std::make_shared<Event::Publisher>();
    auto const spDiscoveryProtocol = std::make_shared<DiscoveryProtocol>(upConfig->GetEndpointOptions());
    auto const spMessageCollector = std::make_shared<AuthorizedProcessor>(upConfig->GetNodeIdentifier());
    auto const spPeerManager = std::make_shared<Peer::Manager>(
        upConfig->GetNodeIdentifier(), upConfig->GetSecurityStrategy(), spDiscoveryProtocol, spMessageCollector);

    spPersistor->SetMediator(spPeerManager.get());

    IBootstrapCache const* const pBootstraps = (options.UseBootstraps()) ? spPersistor.get() : nullptr;
    auto const spNetworkManager = std::make_shared<Network::Manager>(
        upConfig->GetEndpointOptions(), spPeerManager.get(), pBootstraps);

    BryptNode alpha(
        upConfig, spEventPublisher, spNetworkManager, spPeerManager, spMessageCollector, spPersistor);
    assert(alpha.IsInitialized());

    spLogger->info("Welcome to the Brypt Network!");
    spLogger->info("Brypt Identifier: {}", upConfig->GetNodeIdentifier());

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
