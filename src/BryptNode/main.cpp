//----------------------------------------------------------------------------------------------------------------------
// File: main.cpp
// Description:
//----------------------------------------------------------------------------------------------------------------------
#include "BryptNode.hpp"
#include "StartupOptions.hpp"
#include "BryptIdentifier/BryptIdentifier.hpp"
#include "Components/BryptPeer/PeerManager.hpp"
#include "Components/Configuration/Configuration.hpp"
#include "Components/Configuration/ConfigurationManager.hpp"
#include "Components/Configuration/PeerPersistor.hpp"
#include "Components/Event/Publisher.hpp"
#include "Components/MessageControl/AuthorizedProcessor.hpp"
#include "Components/MessageControl/DiscoveryProtocol.hpp"
#include "Components/Network/EndpointTypes.hpp"
#include "Components/Network/EndpointManager.hpp"
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

    auto const upConfigurationManager = std::make_unique<Configuration::Manager>(
            options.GetConfigurationPath(), options.IsInteractive());

    auto const status = upConfigurationManager->FetchSettings();
    if (status != Configuration::StatusCode::Success) {
        spLogger->critical("An unexpected error occured while parsing the configuration file!");
        return 1;
    }

    auto const spBryptIdentifier = upConfigurationManager->GetBryptIdentifier();
    if (!spBryptIdentifier) {
        spLogger->critical("An error occured establishing a Brypt Identifier!");
        return 1;
    }

    auto const optEndpointConfigurations = upConfigurationManager->GetEndpointConfigurations();
    if (!optEndpointConfigurations) {
        spLogger->critical("An error occured parsing endpoint configurations!");
        return 1;
    }

    auto const spPeerPersistor = std::make_shared<PeerPersistor>(options.GetPeersPath(), *optEndpointConfigurations);
    if (!spPeerPersistor->FetchBootstraps()) {
        spLogger->critical("An error occured parsing bootstraps!");
        return 1;
    }

    auto const spEventPublisher = std::make_shared<Event::Publisher>();
    auto const spDiscoveryProtocol = std::make_shared<DiscoveryProtocol>(*optEndpointConfigurations);
    auto const spMessageCollector = std::make_shared<AuthorizedProcessor>(spBryptIdentifier);
    auto const spPeerManager = std::make_shared<PeerManager>(
        spBryptIdentifier, upConfigurationManager->GetSecurityStrategy(),
        spDiscoveryProtocol, spMessageCollector);

    spPeerPersistor->SetMediator(spPeerManager.get());

    IBootstrapCache const* const pBootstraps = (options.UseBootstraps()) ? spPeerPersistor.get() : nullptr;
    auto const spEndpointManager = std::make_shared<EndpointManager>(
        *optEndpointConfigurations, spPeerManager.get(), pBootstraps);

    BryptNode alpha(
        spBryptIdentifier, spEventPublisher, spEndpointManager, spPeerManager, 
        spMessageCollector, spPeerPersistor, upConfigurationManager);

    spLogger->info("Welcome to the Brypt Network!");
    spLogger->info("Brypt Identifier: {}", spBryptIdentifier->GetNetworkRepresentation());

    bool const success = alpha.Startup();
    if (!success) {
        spLogger->critical("An unexpected error caused the node to shutdown!");
        return 1;
    }

    // TODO: Use boost signal_set to listen for SIGTERM, etc.

    return 0;
}

//----------------------------------------------------------------------------------------------------------------------
