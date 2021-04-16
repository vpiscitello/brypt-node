//----------------------------------------------------------------------------------------------------------------------
#include "brypt.h"
#include "BryptIdentifier/BryptIdentifier.hpp"
#include "BryptNode/BryptNode.hpp"
#include "BryptNode/NodeState.hpp"
#include "Components/Configuration/Configuration.hpp"
#include "Components/Configuration/Manager.hpp"
#include "Components/Configuration/PeerPersistor.hpp"
#include "Components/Event/Publisher.hpp"
#include "Components/MessageControl/AuthorizedProcessor.hpp"
#include "Components/MessageControl/DiscoveryProtocol.hpp"
#include "Components/Network/EndpointTypes.hpp"
#include "Components/Network/Manager.hpp"
#include "Utilities/LogUtils.hpp"
#include "Utilities/FileUtils.hpp"
//----------------------------------------------------------------------------------------------------------------------
#include <cstring>
#include <filesystem>
#include <memory>
#include <string>
#include <string_view>
//----------------------------------------------------------------------------------------------------------------------

//----------------------------------------------------------------------------------------------------------------------
namespace {
namespace local {
//----------------------------------------------------------------------------------------------------------------------

//----------------------------------------------------------------------------------------------------------------------
} // local namespace
//----------------------------------------------------------------------------------------------------------------------
namespace defaults {
//----------------------------------------------------------------------------------------------------------------------

constexpr std::string_view ConfigurationFilename = "brypt.config.json";
constexpr std::string_view PeersFilename = "brypt.peers.json";
constexpr bool UseBootstraps = true;

//----------------------------------------------------------------------------------------------------------------------
} // defaults namespace
} // namespace
//----------------------------------------------------------------------------------------------------------------------

static_assert(
    BRYPT_IDENTIFIER_MIN_SIZE == Node::Network::Identifier::MinimumLength, "Brypt Identifier Minimum Size Mismatch!");
static_assert(
    BRYPT_IDENTIFIER_MAX_SIZE == Node::Network::Identifier::MaximumLength, "Brypt Identifier Maximum Size Mismatch!");

//----------------------------------------------------------------------------------------------------------------------

struct brypt_service_t
{
    explicit brypt_service_t(std::filesystem::path&& base_path_arg)
        : base_path(std::move(base_path_arg))
        , configuration_filename(defaults::ConfigurationFilename)
        , peers_filename(defaults::PeersFilename)
        , use_bootstraps(defaults::UseBootstraps)
        , node(nullptr)
    {
    }

    [[nodiscard]] std::filesystem::path GetConfigurationFilepath() const
    {
        return base_path / configuration_filename;
    }

    [[nodiscard]] std::filesystem::path GetPeersFilepath() const
    {
        return base_path / peers_filename;
    }

    std::filesystem::path base_path;
    std::filesystem::path configuration_filename;
    std::filesystem::path peers_filename;
    bool use_bootstraps;
    std::unique_ptr<BryptNode> node;
};

//----------------------------------------------------------------------------------------------------------------------

brypt_service_t* brypt_service_create(char const* base_path, size_t base_path_size)
{
    LogUtils::InitializeLoggers(spdlog::level::off);

    std::filesystem::path path({ base_path, base_path_size });
    if (!std::filesystem::exists(path)) { return nullptr; }

    brypt_service_t* service = new(std::nothrow) brypt_service_t(std::move(path));

    return service;
}

//----------------------------------------------------------------------------------------------------------------------

BRYPT_EXPORT brypt_status_t brypt_service_initialize(brypt_service_t* const service)
{
    if (!service) { return BRYPT_EINVALIDARGUMENT; }
    if (service->node && !service->node->Shutdown()) { return BRYPT_EOPERNOTSUPPORTED; }
   
    auto const upConfig = std::make_unique<Configuration::Manager>(service->GetConfigurationFilepath(), false, false);
    switch (upConfig->FetchSettings()) {
        case Configuration::StatusCode::FileError: return BRYPT_EFILENOTFOUND;
        case Configuration::StatusCode::DecodeError: 
        case Configuration::StatusCode::InputError: return BRYPT_EFILENOTSUPPORTED;
        default: break;
    }

    auto const spIdentifier = upConfig->GetNodeIdentifier();
    if (!spIdentifier) { return BRYPT_EMISSINGPARAMETER; }

    auto const spEventPublisher = std::make_shared<Event::Publisher>();

    auto const optEndpoints = upConfig->GetEndpointConfigurations();
    if (!optEndpoints) { return BRYPT_EMISSINGPARAMETER; }

    auto const spPersistor = std::make_shared<PeerPersistor>(service->GetPeersFilepath(), *optEndpoints, false);
    if (!spPersistor->FetchBootstraps()) { return BRYPT_EMISSINGPARAMETER; }

    auto const spProtocol = std::make_shared<DiscoveryProtocol>(*optEndpoints);
    auto const spProcessor = std::make_shared<AuthorizedProcessor>(spIdentifier);

    auto const spPeerManager = std::make_shared<Peer::Manager>(
        spIdentifier, upConfig->GetSecurityStrategy(), spProtocol, spProcessor);

    spPersistor->SetMediator(spPeerManager.get());

    IBootstrapCache const* const pBootstraps = (service->use_bootstraps) ? spPersistor.get() : nullptr;
    auto const spNetworkManager = std::make_shared<Network::Manager>(*optEndpoints, spPeerManager.get(), pBootstraps);

    service->node = std::make_unique<BryptNode>(
        spIdentifier, spEventPublisher, spNetworkManager,
        spPeerManager, spProcessor, spPersistor, std::move(upConfig));

    return BRYPT_ACCEPTED;
}

//----------------------------------------------------------------------------------------------------------------------

brypt_status_t brypt_service_start(brypt_service_t* const service)
{
    if (!service || !service->node) { return BRYPT_EINVALIDARGUMENT; }
    if (!service->node->Startup<BackgroundRuntime>()) { return BRYPT_EINITNFAILURE; }
    return BRYPT_ACCEPTED;
}

//----------------------------------------------------------------------------------------------------------------------

brypt_status_t brypt_service_stop(brypt_service_t* const service)
{
    if (!service) { return BRYPT_EINVALIDARGUMENT; }
    if (!service->node) { return BRYPT_ENOTSTARTED; }
    
    if (!service->node->Shutdown()) { return BRYPT_EUNSPECIFIED; }
    return BRYPT_ACCEPTED;
}

//----------------------------------------------------------------------------------------------------------------------

brypt_status_t brypt_service_destroy(brypt_service_t* service)
{
    if (!service) { return BRYPT_EINVALIDARGUMENT; }
    if (service->node && !service->node->Shutdown()) { return BRYPT_ESHUTDOWNFAILURE; }

    delete service;
    service = nullptr;

    return BRYPT_ACCEPTED;
}

//----------------------------------------------------------------------------------------------------------------------

brypt_status_t brypt_option_set_int(
    [[maybe_unused]] brypt_service_t* const service,
    [[maybe_unused]] brypt_option_t option, [[maybe_unused]] int32_t value)
{
    return BRYPT_EINVALIDARGUMENT;
}

//----------------------------------------------------------------------------------------------------------------------

brypt_status_t brypt_option_set_bool(brypt_service_t* const service, brypt_option_t option, bool value)
{
    if (!service) { return BRYPT_EINVALIDARGUMENT; }

    switch (option) {
        case BRYPT_OPT_USE_BOOTSTRAPS: { service->use_bootstraps = value; } break;
        default: return BRYPT_EINVALIDARGUMENT;
    }

    return BRYPT_ACCEPTED;
}

//----------------------------------------------------------------------------------------------------------------------

brypt_status_t brypt_option_set_str(
    brypt_service_t* const service, brypt_option_t option, char const* value, size_t size)
{
    if (!service) { return BRYPT_EINVALIDARGUMENT; }
    if (service->node && service->node->IsActive()) { return BRYPT_EOPERNOTSUPPORTED; }

    constexpr auto ValidateFilepath = [] (std::filesystem::path const& path) -> bool
    {
        if (!std::filesystem::exists(path) || path.has_filename()) { return false; }
        return true;
    };

    constexpr auto ValidateFilename = [] (std::filesystem::path const& path) -> bool
    {
        constexpr std::string_view RequiredFileExtension = ".json";
        if (!path.has_filename() || !path.has_extension()) { return false; }
        if (auto const extension = path.extension(); extension != RequiredFileExtension) { return false; }
        return true;
    };
    
    try {
        switch (option) {
            case BRYPT_OPT_BASE_FILEPATH: {
                service->base_path = std::filesystem::path({value, size});
                if (!ValidateFilepath(service->base_path)) {
                    service->base_path.clear();
                    return BRYPT_EINVALIDARGUMENT;
                }
            } break;
            case BRYPT_OPT_CONFIGURATION_FILENAME: {
                service->configuration_filename = std::filesystem::path(std::string_view{value, size});
                if (!ValidateFilename(service->configuration_filename)) { 
                    service->configuration_filename.clear();
                    return BRYPT_EINVALIDARGUMENT;
                }
            } break;
            case BRYPT_OPT_PEERS_FILENAME: {
                service->peers_filename = std::filesystem::path(std::string_view{value, size});
                if (!ValidateFilename(service->peers_filename)) {
                    service->peers_filename.clear();
                    return BRYPT_EINVALIDARGUMENT;
                }
            } break;
            default: return BRYPT_EINVALIDARGUMENT;
        }
    } catch (...) { return BRYPT_EINVALIDARGUMENT; }

    return BRYPT_ACCEPTED;
}

//----------------------------------------------------------------------------------------------------------------------

int32_t brypt_option_get_int(
    [[maybe_unused]] brypt_service_t const* const service, [[maybe_unused]] brypt_option_t option)
{
    return -1;
}

//----------------------------------------------------------------------------------------------------------------------

bool brypt_option_get_bool(brypt_service_t const* const service, brypt_option_t option)
{
    if (!service) { return false; }

    switch (option) {
        case BRYPT_OPT_USE_BOOTSTRAPS: return service->use_bootstraps;
        default: return false;
    }
}

//----------------------------------------------------------------------------------------------------------------------

char const* brypt_option_get_str(brypt_service_t const* const service, brypt_option_t option)
{
    if (!service) { return nullptr; }

    switch (option) {
        case BRYPT_OPT_BASE_FILEPATH: return service->base_path.c_str();
        case BRYPT_OPT_CONFIGURATION_FILENAME: return service->configuration_filename.c_str();
        case BRYPT_OPT_PEERS_FILENAME: return service->peers_filename.c_str();
        default: return nullptr;
    }
}

//----------------------------------------------------------------------------------------------------------------------

bool brypt_service_is_active(brypt_service_t const* const service)
{
    if (!service || !service->node) { return false; }
    return service->node->IsActive();
}

//----------------------------------------------------------------------------------------------------------------------

size_t brypt_service_get_identifier(brypt_service_t const* const service, char* dest, size_t size)
{
    if (!service || !service->node) { return 0; }

    if (auto const spNodeState = service->node->GetNodeState().lock(); spNodeState) {
        auto const spIdentifier = spNodeState->GetNodeIdentifier();
        auto const identifier = spIdentifier->GetNetworkString();
        assert(dest && size >= identifier.size());
        try {
            std::copy(identifier.begin(), identifier.end(), dest);
            return identifier.size();
        } catch(...) {
            return 0;
        }
    }

    return 0;
}

//----------------------------------------------------------------------------------------------------------------------

size_t brypt_service_active_peer_count(brypt_service_t const* const service)
{
    if (!service || !service->node) { return 0; }
    if (auto const spPeerManager = service->node->GetPeerManager().lock(); spPeerManager) {
        return spPeerManager->ActivePeerCount();
    }
    return 0;
}

//----------------------------------------------------------------------------------------------------------------------

size_t brypt_service_inactive_peer_count(brypt_service_t const* const service)
{
    if (!service || !service->node) { return 0; }
    if (auto const spPeerManager = service->node->GetPeerManager().lock(); spPeerManager) {
        return spPeerManager->InactivePeerCount();
    }
    return 0;
}

//----------------------------------------------------------------------------------------------------------------------

size_t brypt_service_observed_peer_count(brypt_service_t const* const service)
{
    if (!service || !service->node) { return 0; }
    if (auto const spPeerManager = service->node->GetPeerManager().lock(); spPeerManager) {
        return spPeerManager->ObservedPeerCount();
    }
    return 0;
}

//----------------------------------------------------------------------------------------------------------------------

char const* brypt_error_description(brypt_status_t code)
{
    switch (code) {
        case BRYPT_ACCEPTED: return "";
        case BRYPT_EUNSPECIFIED: return "Unspecified error";
        case BRYPT_EACCESSDENIED: return "Access denied";
        case BRYPT_EINVALIDARGUMENT: return "Invalid argument";
        case BRYPT_EOPERNOTSUPPORTED: return "Operation not supported";
        case BRYPT_EOPERTIMEOUT: return "Operating timed out";
        case BRYPT_EINITNFAILURE: return "Service could not be initialized";
        case BRYPT_EALREADYSTARTED: return "Service already started";
        case BRYPT_ENOTSTARTED: return "Service is not active";
        case BRYPT_EFILENOTFOUND: return "File could not be found";
        case BRYPT_EFILENOTSUPPORTED: return "File contains an illegal or unsupported option";
        case BRYPT_EMISSINGPARAMETER: return "Configuration failed to apply due to a missing or failed option";
        case BRYPT_ENETBINDFAILED: return "Endpoint could not bind to the specified address";
        case BRYPT_ENETCONNNFAILED: return "Endpoint could not connect to the specified address";
        default: return "Unknown error";
    }
}

//----------------------------------------------------------------------------------------------------------------------
