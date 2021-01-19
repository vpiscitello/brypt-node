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
#include "Utilities/NodeUtils.hpp"
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
    NodeUtils::printo("Welcome to the Brypt Network", NodeUtils::PrintType::Node);

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
        throw std::runtime_error("Error occured parsing settings!");
    }

    auto const spBryptIdentifier = upConfigurationManager->GetBryptIdentifier();
    if (!spBryptIdentifier) {
        throw std::runtime_error("Error occured establishing a Brypt Identifier!");
    }

    NodeUtils::printo(
        "Brypt Identifier: " + spBryptIdentifier->GetNetworkRepresentation(),
        NodeUtils::PrintType::Node);

    auto const optEndpointConfigurations = upConfigurationManager->GetEndpointConfigurations();
    if (!optEndpointConfigurations) {
        throw std::runtime_error("Error occured parsing endpoint configurations!");
    }

    auto const spPeerPersistor = std::make_shared<CPeerPersistor>(
        local::PeersFilename, *optEndpointConfigurations);

    if (!spPeerPersistor->FetchBootstraps()) {
        throw std::runtime_error("Error occured parsing bootstraps!");
    }

    auto const spDiscoveryProtocol = std::make_shared<CDiscoveryProtocol>(
        *optEndpointConfigurations);

    auto const spMessageCollector = std::make_shared<CAuthorizedProcessor>();

    auto const spPeerManager = std::make_shared<CPeerManager>(
        spBryptIdentifier, upConfigurationManager->GetSecurityStrategy(),
        spDiscoveryProtocol, spMessageCollector);

    spPeerPersistor->SetMediator(spPeerManager.get());

    auto const spEndpointManager = std::make_shared<CEndpointManager>(
        *optEndpointConfigurations, spPeerManager.get(), spPeerPersistor.get());

    CBryptNode alpha(
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
