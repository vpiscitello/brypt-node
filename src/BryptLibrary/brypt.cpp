//----------------------------------------------------------------------------------------------------------------------
#include "brypt.h"
#include "service.hpp"
//----------------------------------------------------------------------------------------------------------------------
#include "Components/Configuration/Options.hpp"
#include "Components/Configuration/Parser.hpp"
#include "Components/Configuration/BootstrapService.hpp"
#include "Components/Core/RuntimeContext.hpp"
#include "Components/Event/Publisher.hpp"
#include "Components/Identifier/BryptIdentifier.hpp" 
#include "Components/Network/Address.hpp"
#include "Components/Network/ConnectionState.hpp"
#include "Components/Network/EndpointTypes.hpp"
#include "Components/Network/Manager.hpp"
#include "Components/Network/Protocol.hpp"
#include "Components/Peer/Action.hpp"
#include "Components/Peer/Proxy.hpp"
#include "Components/Peer/ProxyStore.hpp"
#include "Components/Peer/Registration.hpp"
#include "Components/Route/Auxiliary.hpp"
#include "Components/Route/Router.hpp"
#include "Components/Security/SecurityDefinitions.hpp"
#include "Components/State/NodeState.hpp"
#include "Utilities/Assertions.hpp"
#include "Utilities/Logger.hpp"
//----------------------------------------------------------------------------------------------------------------------
#include <cstring>
#include <filesystem>
#include <memory>
#include <random>
#include <string>
#include <string_view>
#include <optional>
//----------------------------------------------------------------------------------------------------------------------

//----------------------------------------------------------------------------------------------------------------------
namespace {
namespace local {
//----------------------------------------------------------------------------------------------------------------------

using BindingFailure = Event::Message<Event::Type::BindingFailed>::Cause;
using ConnectionFailure = Event::Message<Event::Type::ConnectionFailed>::Cause;

brypt_confidentiality_level_t translate_confidentiality_level(Security::ConfidentialityLevel level);
Security::ConfidentialityLevel translate_confidentiality_level(brypt_confidentiality_level_t level);
brypt_protocol_t translate_protocol(Network::Protocol protocol);
Network::Protocol translate_protocol(brypt_protocol_t protocol);
brypt_connection_state_t translate_connection_state(Network::Connection::State state);
brypt_authorization_state_t translate_authorization_state(Security::State state);
brypt_result_t translate_binding_failure(BindingFailure failure);
brypt_result_t translate_connection_failure(ConnectionFailure failure);
brypt_request_key_t translate_tracker_key(Awaitable::TrackerKey const& key);
brypt_status_code_t translate_status_code(Message::Extension::Status::Code code);
Message::Extension::Status::Code translate_status_code(brypt_status_code_t code);

//----------------------------------------------------------------------------------------------------------------------
} // local namespace
} // namespace
//----------------------------------------------------------------------------------------------------------------------

static_assert(BRYPT_IDENTIFIER_MIN_SIZE == Node::Identifier::MinimumSize, "Brypt Identifier Minimum Size Mismatch!");
static_assert(BRYPT_IDENTIFIER_MAX_SIZE == Node::Identifier::MaximumSize, "Brypt Identifier Maximum Size Mismatch!");

static_assert(BRYPT_REQUEST_KEY_SIZE == Awaitable::TrackerKey{ 0 }.size(), "Request Key Size Mismatch!");

//----------------------------------------------------------------------------------------------------------------------

struct brypt_next_key_t
{
public:
    explicit brypt_next_key_t(Peer::Action::Next& next) : m_next(next) { }
    Peer::Action::Next& m_next;
};

//----------------------------------------------------------------------------------------------------------------------

brypt_service_t* brypt_service_create() noexcept
{
    assert(Assertions::Threading::RegisterCoreThread());

    brypt_service_t* service = nullptr;
    try { 
        Logger::Initialize(spdlog::level::off, false);

        service = new brypt_service_t;
        service->create_core();
    }
    catch (...) { if (service) { brypt_service_destroy(service); }  }

    return service;
}

//----------------------------------------------------------------------------------------------------------------------

brypt_result_t brypt_service_start(brypt_service_t* const service) noexcept
{
    if (brypt_service_t::not_created(service)) { return BRYPT_EINVALIDARGUMENT; }
    try {
        return service->startup();
    } 
    catch ([[maybe_unused]] std::bad_alloc const& exception) { return BRYPT_EOUTOFMEMORY; }
    catch (...) { return BRYPT_EUNSPECIFIED; }
}

//----------------------------------------------------------------------------------------------------------------------

brypt_result_t brypt_service_stop(brypt_service_t* const service) noexcept
{
    if (brypt_service_t::not_created(service)) { return BRYPT_EINVALIDARGUMENT; }
    try {
        service->shutdown();
        return BRYPT_ACCEPTED;
    } 
    catch ([[maybe_unused]] std::bad_alloc const& exception) { return BRYPT_EOUTOFMEMORY; }
    catch (...) { return BRYPT_EUNSPECIFIED; }
}

//----------------------------------------------------------------------------------------------------------------------

brypt_result_t brypt_service_restart(brypt_service_t* const service) noexcept
{
    if (brypt_service_t::not_created(service)) { return BRYPT_EINVALIDARGUMENT; }
    try {
        service->shutdown();
        return service->startup();
    } 
    catch ([[maybe_unused]] std::bad_alloc const& exception) { return BRYPT_EOUTOFMEMORY; }
    catch (...) { return BRYPT_EUNSPECIFIED; }
}

//----------------------------------------------------------------------------------------------------------------------

brypt_result_t brypt_service_destroy(brypt_service_t* service) noexcept
{
    if (brypt_service_t::not_created(service)) { return BRYPT_EINVALIDARGUMENT; }
    brypt_service_stop(service); // Ensure the core has been stopped before destroying the resources. 

    delete service;
    service = nullptr;

    return BRYPT_ACCEPTED;
}

//----------------------------------------------------------------------------------------------------------------------

bool brypt_option_get_bool(brypt_service_t const* const service, brypt_option_t option) noexcept
{
    if (brypt_service_t::not_created(service)) { return false; }

    switch (option) {
        case BRYPT_OPT_USE_BOOTSTRAPS: return service->get_use_bootstraps();
        default: return false;
    }
}

//----------------------------------------------------------------------------------------------------------------------

int32_t brypt_option_get_int32(brypt_service_t const* const service, brypt_option_t option) noexcept
{
    if (brypt_service_t::not_created(service)) { return -1; }

    switch (option) {
        case BRYPT_OPT_CORE_THREADS: return service->get_core_threads();
        case BRYPT_OPT_IDENTIFIER_PERSISTENCE: return service->get_identifier_persistence(); 
        //case BRYPT_OPT_SECURITY_STRATEGY: return service->get_security_strategy();
        case BRYPT_OPT_LOG_LEVEL: return service->get_log_level();
        case BRYPT_OPT_CONNECTION_TIMEOUT: {
            auto const timeout = service->get_connection_timeout();
            if (!std::in_range<std::int32_t>(timeout.count())) { return -1; }
            return static_cast<std::int32_t>(timeout.count());
        }
        case BRYPT_OPT_CONNECTION_RETRY_LIMIT: return service->get_connection_retry_limit();
        case BRYPT_OPT_CONNECTION_RETRY_INTERVAL:{
            auto const interval = service->get_connection_retry_interval();
            if (!std::in_range<std::int32_t>(interval.count())) { return -1; }
            return static_cast<std::int32_t>(interval.count());
        }
        default: return -1;
    }
}

//----------------------------------------------------------------------------------------------------------------------

char const* brypt_option_get_string(brypt_service_t const* const service, brypt_option_t option) noexcept
{
    if (brypt_service_t::not_created(service)) { return nullptr; }
    
    switch (option) {
        case BRYPT_OPT_BASE_FILEPATH: return service->get_base_path();
        case BRYPT_OPT_CONFIGURATION_FILENAME: return service->get_configuration_filename();
        case BRYPT_OPT_BOOTSTRAP_FILENAME: return service->get_bootstrap_filename();
        case BRYPT_OPT_NODE_NAME: return service->get_node_name().c_str();
        case BRYPT_OPT_NODE_DESCRIPTION: return service->get_node_description().c_str();
        default: return nullptr;
    }
}

//----------------------------------------------------------------------------------------------------------------------

brypt_result_t brypt_option_set_bool(brypt_service_t* const service, brypt_option_t option, bool value) noexcept
{
    if (brypt_service_t::not_created(service)) { return BRYPT_EINVALIDARGUMENT; }
    if (brypt_service_t::active(service)) { return BRYPT_ENOTSUPPORTED; }

    switch (option) {
        case BRYPT_OPT_USE_BOOTSTRAPS: service->set_use_bootstraps(value); break;
        default: return BRYPT_EINVALIDARGUMENT;
    }

    return BRYPT_ACCEPTED;
}

//----------------------------------------------------------------------------------------------------------------------

brypt_result_t brypt_option_set_int32(brypt_service_t* const service, brypt_option_t option, int32_t value) noexcept
{
    if (brypt_service_t::not_created(service)) { return BRYPT_EINVALIDARGUMENT; }
    if (brypt_service_t::active(service)) { return BRYPT_ENOTSUPPORTED; }

    switch (option) {
        case BRYPT_OPT_CORE_THREADS: return service->set_core_threads(value);
        case BRYPT_OPT_IDENTIFIER_PERSISTENCE: return service->set_identifier_persistence(value);
        //case BRYPT_OPT_SECURITY_STRATEGY: return service->set_security_strategy(value);
        case BRYPT_OPT_LOG_LEVEL: return service->set_log_level(value);
        case BRYPT_OPT_CONNECTION_TIMEOUT: return service->set_connection_timeout(value);
        case BRYPT_OPT_CONNECTION_RETRY_LIMIT: return service->set_connection_retry_limit(value);
        case BRYPT_OPT_CONNECTION_RETRY_INTERVAL: return service->set_connection_retry_interval(value);
        default: return BRYPT_EINVALIDARGUMENT;
    }
}

//----------------------------------------------------------------------------------------------------------------------

brypt_result_t brypt_option_set_string(
    brypt_service_t* const service, brypt_option_t option, char const* value, size_t size) noexcept
{
    if (brypt_service_t::not_created(service)) { return BRYPT_EINVALIDARGUMENT; }
    if (brypt_service_t::active(service)) { return BRYPT_ENOTSUPPORTED; }

    try {
        switch (option) {
            case BRYPT_OPT_BASE_FILEPATH: return service->set_base_path({value, size});
            case BRYPT_OPT_CONFIGURATION_FILENAME: return service->set_configuration_filename({value, size});
            case BRYPT_OPT_BOOTSTRAP_FILENAME: return service->set_bootstrap_filename({value, size});
            case BRYPT_OPT_NODE_NAME: return service->set_node_name({value, size});
            case BRYPT_OPT_NODE_DESCRIPTION: return service->set_node_description({value, size});
            default: return BRYPT_EINVALIDARGUMENT;
        }
    }
    catch ([[maybe_unused]] std::bad_alloc const& exception) { return BRYPT_EOUTOFMEMORY; }
    catch (...) { return BRYPT_EUNSPECIFIED; }
}

//----------------------------------------------------------------------------------------------------------------------

bool brypt_option_is_bool_type(brypt_option_t option) noexcept
{
    switch (option) {
        case BRYPT_OPT_USE_BOOTSTRAPS: return true;
        default: return false;
    }
}

//----------------------------------------------------------------------------------------------------------------------

bool brypt_option_is_int_type(brypt_option_t option) noexcept
{
    switch (option) {
        case BRYPT_OPT_CORE_THREADS: 
        case BRYPT_OPT_IDENTIFIER_PERSISTENCE: 
        case BRYPT_OPT_SECURITY_STRATEGY: 
        case BRYPT_OPT_LOG_LEVEL: 
        case BRYPT_OPT_CONNECTION_TIMEOUT: 
        case BRYPT_OPT_CONNECTION_RETRY_LIMIT: 
        case BRYPT_OPT_CONNECTION_RETRY_INTERVAL: return true;
        default: return false;
    }
}

//----------------------------------------------------------------------------------------------------------------------

bool brypt_option_is_string_type(brypt_option_t option) noexcept
{
    switch (option) {
        case BRYPT_OPT_BASE_FILEPATH:
        case BRYPT_OPT_CONFIGURATION_FILENAME:
        case BRYPT_OPT_BOOTSTRAP_FILENAME: 
        case BRYPT_OPT_NODE_NAME: 
        case BRYPT_OPT_NODE_DESCRIPTION: return true;
        default: return false;
    }
}

//----------------------------------------------------------------------------------------------------------------------

brypt_result_t brypt_option_read_supported_algorithms(
    brypt_service_t const* const service, brypt_option_supported_algorithms_reader_t callback, void* context) noexcept
{
    if (brypt_service_t::not_created(service)) { return BRYPT_EINVALIDARGUMENT; }
    try {
        using Security::ConfidentialityLevel;
        using Configuration::Options::Algorithms;

        auto const& supportedAlgorithms = service->get_supported_algorithms();
        bool const result = supportedAlgorithms.ForEachSupportedAlgorithm([&] (ConfidentialityLevel level, Algorithms const& algorithms) {
            auto const _level = local::translate_confidentiality_level(level);

            std::vector<char const*> key_agreements;
            key_agreements.reserve(algorithms.GetKeyAgreements().size());
            std::ranges::transform(algorithms.GetKeyAgreements(), std::back_inserter(key_agreements), [] (std::string const& name) {
                return name.c_str();
            });

            std::vector<char const*> ciphers;
            ciphers.reserve(algorithms.GetCiphers().size());
            std::ranges::transform(algorithms.GetCiphers(), std::back_inserter(ciphers), [] (std::string const& name) {
                return name.c_str();
            });

            std::vector<char const*> hash_functions;
            hash_functions.reserve(algorithms.GetHashFunctions().size());
            std::ranges::transform(algorithms.GetHashFunctions(), std::back_inserter(hash_functions), [] (std::string const& name) {
                return name.c_str();
            });

            brypt_option_algorithms_package_t package = {
                .level = _level,
                .key_agreements = key_agreements.data(),
                .key_agreements_size = key_agreements.size(),
                .ciphers = ciphers.data(),
                .ciphers_size = ciphers.size(),
                .hash_functions = hash_functions.data(),
                .hash_functions_size = hash_functions.size()
            };

            if (!callback(&package, context)) { return CallbackIteration::Stop; }

            return CallbackIteration::Continue;
        });

        if (!result) {
            return BRYPT_ECANCELED;
        }
    }
    catch ([[maybe_unused]] std::bad_alloc const& exception) { return BRYPT_EOUTOFMEMORY; }
    catch (...) { return BRYPT_EUNSPECIFIED; }

    return BRYPT_ACCEPTED;
}

//----------------------------------------------------------------------------------------------------------------------

brypt_result_t brypt_option_clear_supported_algorithms(brypt_service_t* const service) noexcept
{
    if (brypt_service_t::not_created(service)) { return BRYPT_EINVALIDARGUMENT; }
    try {
        service->clear_supported_algorithms();
    }
    catch ([[maybe_unused]] std::bad_alloc const& exception) { return BRYPT_EOUTOFMEMORY; }
    catch (...) { return BRYPT_EUNSPECIFIED; }

    return BRYPT_ACCEPTED;
}

//----------------------------------------------------------------------------------------------------------------------

brypt_result_t brypt_option_set_supported_algorithms(
    brypt_service_t* const service,
    brypt_option_algorithms_package_t const* const* const entries,
    size_t size) noexcept
{
    if (brypt_service_t::not_created(service)) { return BRYPT_EINVALIDARGUMENT; }
    try {
        for (std::size_t idx = 0; idx < size; ++idx) {
            if (entries[idx] != nullptr) {
                brypt_option_algorithms_package_t const* const entry = entries[idx];
                brypt_result_t result = brypt_option_set_algorithms_for_level(
                    service,
                    entry->level,
                    entry->key_agreements,
                    entry->key_agreements_size,
                    entry->ciphers,
                    entry->ciphers_size,
                    entry->hash_functions,
                    entry->hash_functions_size);

                if (result != BRYPT_ACCEPTED) {
                    return result;
                }
            } else {
                return BRYPT_EINVALIDARGUMENT;
            }
        }
    }
    catch ([[maybe_unused]] std::bad_alloc const& exception) { return BRYPT_EOUTOFMEMORY; }
    catch (...) { return BRYPT_EUNSPECIFIED; }

    return BRYPT_ACCEPTED;
}

//----------------------------------------------------------------------------------------------------------------------

brypt_result_t brypt_option_set_algorithms_for_level(
    brypt_service_t* const service,
    brypt_confidentiality_level_t _level,
    char const* const* const _key_agreements,
    size_t key_agreements_size,
    char const* const* const _ciphers,
    size_t ciphers_size,
    char const* const* const _hash_functions,
    size_t hash_functions_size) noexcept
{
    if (brypt_service_t::not_created(service)) { return BRYPT_EINVALIDARGUMENT; }
    try {
        auto const level = local::translate_confidentiality_level(_level);
        if (level == Security::ConfidentialityLevel::Unknown) { return BRYPT_EINVALIDARGUMENT; }

        std::vector<std::string> key_agreements;
        for (std::size_t idx = 0; idx < key_agreements_size; ++idx) {
            if (_key_agreements[idx] != nullptr) {
                key_agreements.emplace_back(_key_agreements[idx]);
            } else {
                return BRYPT_EINVALIDARGUMENT;
            }
        }

        std::vector<std::string> ciphers;
        for (std::size_t idx = 0; idx < ciphers_size; ++idx) {
            if (_ciphers[idx] != nullptr) {
                ciphers.emplace_back(_ciphers[idx]);
            } else {
                return BRYPT_EINVALIDARGUMENT;
            }
        }

        std::vector<std::string> hash_functions;
        for (std::size_t idx = 0; idx < hash_functions_size; ++idx) {
            if (_hash_functions[idx] != nullptr) {
                hash_functions.emplace_back(_hash_functions[idx]);
            } else {
                return BRYPT_EINVALIDARGUMENT;
            }
        }

        return service->set_supported_algorithms(
            level, std::move(key_agreements), std::move(ciphers), std::move(hash_functions));
    }
    catch ([[maybe_unused]] std::bad_alloc const& exception) { return BRYPT_EOUTOFMEMORY; }
    catch (...) { return BRYPT_EUNSPECIFIED; }

    return BRYPT_ACCEPTED;
}

//----------------------------------------------------------------------------------------------------------------------

size_t brypt_option_get_endpoint_count(brypt_service_t const* const service) noexcept
{
    if (brypt_service_t::not_created(service)) { return BRYPT_EINVALIDARGUMENT; }
    return service->get_endpoint_count();
}

//----------------------------------------------------------------------------------------------------------------------

brypt_result_t brypt_option_read_endpoints(
    brypt_service_t const* const service, brypt_option_endpoint_reader_t callback, void* context) noexcept
{
    if (brypt_service_t::not_created(service)) { return BRYPT_EINVALIDARGUMENT; }
    try {
        for (auto const& endpoint : service->get_endpoints()) {
            auto const& interface = endpoint.GetInterface();
            auto const& binding = endpoint.GetBindingString();
            auto const& optBootstrap = endpoint.GetBootstrap();

            brypt_option_endpoint_t _endpoint = {
                .protocol = local::translate_protocol(endpoint.GetProtocol()),
                .interface = interface.c_str(),
                .binding = binding.c_str(),
                .bootstrap = (optBootstrap.has_value()) ? optBootstrap->GetAuthority().data() : nullptr
            };

            if (!callback(&_endpoint, context)) { return BRYPT_ECANCELED; }
        }
    }
    catch ([[maybe_unused]] std::bad_alloc const& exception) { return BRYPT_EOUTOFMEMORY; }
    catch (...) { return BRYPT_EUNSPECIFIED; }

    return BRYPT_ACCEPTED;
}

//----------------------------------------------------------------------------------------------------------------------

brypt_result_t brypt_option_find_endpoint(
    brypt_service_t const* const service,
    brypt_protocol_t _protocol,
    char const* binding,
    brypt_option_endpoint_reader_t callback,
    void* context) noexcept
{
    if (brypt_service_t::not_created(service)) { return BRYPT_EINVALIDARGUMENT; }
    try {
        auto const protocol = local::translate_protocol(_protocol);
        if (protocol == Network::Protocol::Invalid || !binding) { return BRYPT_EINVALIDARGUMENT; }

        auto const& endpoints = service->get_endpoints();
        auto const itr = std::ranges::find_if(endpoints, [&] (auto const& options) {
            // We can match to the configured binding or the constructed binding. 
            bool const found = protocol == options.GetProtocol() && 
                (binding == options.GetBindingString() || binding == options.GetBinding().GetAuthority());
            return found;
        });

        if (itr == endpoints.end()) { return BRYPT_ENOTFOUND; }

        auto const& interface = itr->GetInterface();
        auto const& binding = itr->GetBindingString();
        auto const& optBootstrap = itr->GetBootstrap();

        brypt_option_endpoint_t endpoint = {
            .protocol = _protocol,
            .interface = interface.c_str(),
            .binding = binding.c_str(),
            .bootstrap = (optBootstrap.has_value()) ? optBootstrap->GetAuthority().data() : nullptr
        };

        if (!callback(&endpoint, context)) { return BRYPT_ECANCELED; }
    }
    catch ([[maybe_unused]] std::bad_alloc const& exception) { return BRYPT_EOUTOFMEMORY; }
    catch (...) { return BRYPT_EUNSPECIFIED; }

    return BRYPT_ACCEPTED;
}

//----------------------------------------------------------------------------------------------------------------------

brypt_result_t brypt_option_attach_endpoint(
    brypt_service_t* const service, brypt_option_endpoint_t const* const _options) noexcept
{
    if (brypt_service_t::not_created(service)) { return BRYPT_EINVALIDARGUMENT; }
    if (!_options) { return BRYPT_EINVALIDARGUMENT; }

    brypt_result_t result = BRYPT_ACCEPTED;
    try {
        auto const protocol = local::translate_protocol(_options->protocol);
        if (protocol == Network::Protocol::Invalid) { return BRYPT_EINVALIDARGUMENT; }

        Configuration::Options::Endpoint options = {
            protocol,
            (_options->interface) ? _options->interface : "",
            (_options->binding) ? _options->binding : "",
            (_options->bootstrap) ? std::optional<std::string>{ _options->bootstrap } : std::nullopt
        };

        // Before performing any operations using the provided options, ensure that all fields are valid. 
        if (auto const [result, message] = options.AreOptionsAllowable(); result != Configuration::StatusCode::Success) {
            return BRYPT_EINVALIDARGUMENT;
        }

        result = service->attach_endpoint(std::move(options));
    }
    catch ([[maybe_unused]] std::bad_alloc const& exception) { return BRYPT_EOUTOFMEMORY; }
    catch (...) { return BRYPT_EINVALIDARGUMENT; }

    return result;
}

//----------------------------------------------------------------------------------------------------------------------

brypt_result_t brypt_option_detach_endpoint(
    brypt_service_t* const service, brypt_protocol_t _protocol, char const* binding) noexcept
{
    if (brypt_service_t::not_created(service)) { return BRYPT_EINVALIDARGUMENT; }

    auto const protocol = local::translate_protocol(_protocol);
    if (protocol == Network::Protocol::Invalid || !binding) { return BRYPT_EINVALIDARGUMENT; }

    brypt_result_t result = BRYPT_ACCEPTED;
    try {
        result = service->detach_endpoint(protocol, binding);
    }
    catch ([[maybe_unused]] std::bad_alloc const& exception) { return BRYPT_EOUTOFMEMORY; }
    catch (...) { return BRYPT_EUNSPECIFIED; }

    return result;
}

//----------------------------------------------------------------------------------------------------------------------

bool brypt_option_configuration_file_exists(brypt_service_t const* const service) noexcept
{
    if (brypt_service_t::not_created(service)) { return BRYPT_EINVALIDARGUMENT; }
    return service->configuration_file_exists();
}

//----------------------------------------------------------------------------------------------------------------------
bool brypt_option_configuration_validated(brypt_service_t const* const service) noexcept
{
    if (brypt_service_t::not_created(service)) { return BRYPT_EINVALIDARGUMENT; }
    return service->configuration_validated();
}

//----------------------------------------------------------------------------------------------------------------------

bool brypt_service_is_active(brypt_service_t const* const service) noexcept
{
    return brypt_service_t::active(service);
}

//----------------------------------------------------------------------------------------------------------------------

size_t brypt_service_get_identifier(brypt_service_t* const service, char* dest, size_t size) noexcept
{
    if (brypt_service_t::not_created(service)) { return 0; }

    try {
        auto const& identifier = service->get_identifier();
        if (dest && size >= identifier.size()) {
            std::copy(identifier.begin(), identifier.end(), dest);
            return identifier.size();
        }
    } catch(...) { return 0; }

    return 0;
}

//----------------------------------------------------------------------------------------------------------------------

bool brypt_service_is_peer_connected(
    brypt_service_t const* const service, char const* identifier) noexcept
{
    if (brypt_service_t::not_created(service)) { return false; }
    if (!identifier) { return false; }

    try {
        if (auto const spProxyStore = service->get_proxy_store(); spProxyStore) {
            auto const spProxy = spProxyStore->Find(identifier);
            return (spProxy) ? spProxy->IsActive() : false;
        }
    } catch(...) { return false; }

    return false;
}

//----------------------------------------------------------------------------------------------------------------------
 
brypt_result_t brypt_service_get_peer_statistics(
    brypt_service_t const* const service,
    char const* identifier,
    brypt_peer_statistics_reader_t callback,
    void* context) noexcept
{
    if (brypt_service_t::not_created(service)) { return BRYPT_EINVALIDARGUMENT; }
    if (!identifier) { return BRYPT_EINVALIDARGUMENT; }

    try {
        if (auto const spProxyStore = service->get_proxy_store(); spProxyStore) {
            auto const spProxy = spProxyStore->Find(identifier);
            if (!spProxy) { return BRYPT_ENOTFOUND; }
            
            brypt_peer_statistics_t statistics = {
                .sent = 0,
                .received = 0
            };

            statistics.sent = spProxy->GetSentCount();
            statistics.received = spProxy->GetReceivedCount();

            if (!callback(&statistics, context)) { return BRYPT_ECANCELED; }
            return BRYPT_ACCEPTED;
        }
    }
    catch ([[maybe_unused]] std::bad_alloc const& exception) { return BRYPT_EOUTOFMEMORY; }
    catch (...) { return BRYPT_EUNSPECIFIED; }

    return BRYPT_ENOTAVAILABLE;
}

//----------------------------------------------------------------------------------------------------------------------

brypt_result_t brypt_service_get_peer_details(
    brypt_service_t const* const service,
    char const* identifier,
    brypt_peer_details_reader_t callback,
    void* context) noexcept
{
    if (brypt_service_t::not_created(service)) { return BRYPT_EINVALIDARGUMENT; }
    if (!identifier || !callback) { return BRYPT_EINVALIDARGUMENT; }

    try {
        if (auto const spProxyStore = service->get_proxy_store(); spProxyStore) {
            auto const spProxy = spProxyStore->Find(identifier);
            if (!spProxy) { return BRYPT_ENOTFOUND; }
            
            bool const active = spProxy->IsActive();

            std::vector<Network::RemoteAddress> remotes;
            if (active) {
                spProxy->ForEach([&remotes] (Peer::Registration const& registration) {
                    remotes.emplace_back(registration.GetAddress());
                    return CallbackIteration::Continue;
                });
            }

            std::vector<brypt_remote_address_t> _remotes;
            std::ranges::transform(remotes, std::back_inserter(_remotes), [](Network::RemoteAddress const& remote) {
                return brypt_remote_address_t{
                    .protocol = local::translate_protocol(remote.GetProtocol()),
                    .uri = remote.GetUri().c_str(),
                    .size = remote.GetSize(),
                    .bootstrapable = remote.IsBootstrapable()
                };
            });

            brypt_peer_details_t details = {
                .connection_state = (active) ? BRYPT_CONNECTED_STATE : BRYPT_DISCONNECTED_STATE,
                .authorization_state = local::translate_authorization_state(spProxy->GetAuthorization()),
                .remotes = _remotes.data(),
                .remotes_size = _remotes.size(),
                .statistics = brypt_peer_statistics_t{ 
                    .sent = spProxy->GetSentCount(),
                    .received = spProxy->GetReceivedCount()
                },
            };

            if (!callback(&details, context)) { return BRYPT_ECANCELED; }
            return BRYPT_ACCEPTED;
        }
    }
    catch ([[maybe_unused]] std::bad_alloc const& exception) { return BRYPT_EOUTOFMEMORY; }
    catch (...) { return BRYPT_EUNSPECIFIED; }

    return BRYPT_ENOTAVAILABLE;
}

//----------------------------------------------------------------------------------------------------------------------

size_t brypt_service_active_peer_count(brypt_service_t const* const service) noexcept
{
    if (brypt_service_t::not_created(service)) { return 0; }
    
    try {
        if (auto const spProxyStore = service->get_proxy_store(); spProxyStore) {
            return spProxyStore->ActiveCount();
        }
    }
    catch(...) { return 0; }
    
    return 0;
}

//----------------------------------------------------------------------------------------------------------------------

size_t brypt_service_inactive_peer_count(brypt_service_t const* const service) noexcept
{
    if (brypt_service_t::not_created(service)) { return 0; }
    
    try {
        if (auto const spProxyStore = service->get_proxy_store(); spProxyStore) {
            return spProxyStore->InactiveCount();
        }
    }
    catch(...) { return 0; }

    return 0;
}

//----------------------------------------------------------------------------------------------------------------------

size_t brypt_service_observed_peer_count(brypt_service_t const* const service) noexcept
{
    if (brypt_service_t::not_created(service)) { return 0; }

    try {
        if (auto const spProxyStore = service->get_proxy_store(); spProxyStore) {
            return spProxyStore->ObservedCount();
        }
    }
    catch(...) { return 0; }

    return 0;
}

//----------------------------------------------------------------------------------------------------------------------

brypt_result_t brypt_service_register_route(
    brypt_service_t* const service, char const* route, brypt_on_message_t callback, void* context) noexcept
{
    if (brypt_service_t::not_created(service)) { return BRYPT_EINVALIDARGUMENT; }

    try {
        if (auto const spRouter = service->get_router(); spRouter) {
            bool const registered = spRouter->Register<Route::Auxiliary::ExternalHandler>(route, 
                [callback, context] (Message::Application::Parcel const& message, Peer::Action::Next& next) -> bool {
                    char const* source = static_cast<std::string const&>(message.GetSource()).c_str();
                    auto const readable = message.GetPayload().GetReadableView();
                    brypt_next_key_t key{ next };
                    return callback(source, readable.data(), readable.size(), &key, context);
                });
            return (registered) ? BRYPT_ACCEPTED : BRYPT_EINVALIDARGUMENT;
        }
    }
    catch ([[maybe_unused]] std::bad_alloc const& exception) { return BRYPT_EOUTOFMEMORY; }
    catch(...) { return BRYPT_EUNSPECIFIED; }

    return BRYPT_ENOTAVAILABLE;
}

//----------------------------------------------------------------------------------------------------------------------

brypt_result_t brypt_service_connect(
    brypt_service_t const* const service, brypt_protocol_t _protocol, char const* address) noexcept
{
    if (brypt_service_t::not_initialized(service)) { return BRYPT_EINVALIDARGUMENT; }

    auto const protocol = local::translate_protocol(_protocol);
    if (protocol == Network::Protocol::Invalid || !address || sizeof(address) == 0) { 
        return BRYPT_EINVALIDARGUMENT; 
    }

    try {
        using Origin = Network::RemoteAddress::Origin;
        auto const remote = Network::RemoteAddress{ protocol, address, true, Origin::User };
        if (!remote.IsValid()) { return BRYPT_EINVALIDARGUMENT; }
        if (auto const spNetworkManager = service->get_network_manager(); spNetworkManager) {
            bool const success = spNetworkManager->ScheduleConnect(remote);
            return (success) ? BRYPT_ACCEPTED : BRYPT_EUNSPECIFIED;
        }
    }
    catch ([[maybe_unused]] std::bad_alloc const& exception) { return BRYPT_EOUTOFMEMORY; }
    catch (...) { return BRYPT_EUNSPECIFIED; }

    return BRYPT_ENOTAVAILABLE;
}

//----------------------------------------------------------------------------------------------------------------------

brypt_result_t brypt_service_disconnect_by_identifier(brypt_service_t const* const service, char const* identifier) noexcept
{
    if (brypt_service_t::not_initialized(service)) { return BRYPT_EINVALIDARGUMENT; }
    if (!identifier || sizeof(identifier) == 0) { return BRYPT_EINVALIDARGUMENT; }

    try {
        if (auto const spProxyStore = service->get_proxy_store(); spProxyStore) {
            bool const success = spProxyStore->ScheduleDisconnect(identifier);
            return (success) ? BRYPT_ACCEPTED : BRYPT_ENOTFOUND;
        }
    }
    catch ([[maybe_unused]] std::bad_alloc const& exception) { return BRYPT_EOUTOFMEMORY; }
    catch (...) { return BRYPT_EUNSPECIFIED; }

    return BRYPT_ENOTAVAILABLE;
}

//----------------------------------------------------------------------------------------------------------------------

size_t brypt_service_disconnect_by_address(
    brypt_service_t const* const service, brypt_protocol_t _protocol, char const* address) noexcept
{
    if (brypt_service_t::not_initialized(service)) { return BRYPT_EINVALIDARGUMENT; }

    auto const protocol = local::translate_protocol(_protocol);
    if (protocol == Network::Protocol::Invalid || !address || sizeof(address) == 0) {  return BRYPT_EINVALIDARGUMENT; }

    try {
        // Note: It assumed that the library user only knows the bootstrappable addresses of peers. If the provided
        // address is registered to multiple peers (non-unique), all matching peers will be disconnected. 
        using Origin = Network::RemoteAddress::Origin;
        auto const remote = Network::RemoteAddress{ protocol, address, true, Origin::User };
        if (!remote.IsValid()) { return BRYPT_EINVALIDARGUMENT; }

        if (auto const spProxyStore = service->get_proxy_store(); spProxyStore) {
            return spProxyStore->ScheduleDisconnect(remote);
        }
    }
    catch (...) { return 0; }

    return 0;
}

//----------------------------------------------------------------------------------------------------------------------

brypt_dispatch_t brypt_service_dispatch(
    brypt_service_t const* const service,
    char const* identifier,
    char const* route,
    uint8_t const* _payload,
    size_t size) noexcept
{
    brypt_dispatch_t result { 
        .dispatched = 0,
        .result = BRYPT_EINVALIDARGUMENT
    };

    if (brypt_service_t::not_initialized(service)) { return result; }
    if (!identifier || !route) { return result; }

    result.result = BRYPT_ENOTAVAILABLE;
    try {
        if (auto const spProxyStore = service->get_proxy_store(); spProxyStore) {
            auto payload = Message::Payload{ std::span{ _payload, size } };
            bool const success = spProxyStore->Dispatch(identifier, route, std::move(payload));
            if (success) {
                result.dispatched = 1;
                result.result = BRYPT_ACCEPTED;
            } else {
                result.result = BRYPT_ENOTAVAILABLE;
            }
        }
    }
    catch ([[maybe_unused]] std::bad_alloc const& exception) { result.result = BRYPT_EOUTOFMEMORY; }
    catch (...) { result.result = BRYPT_EUNSPECIFIED; }

    return result;
}

//----------------------------------------------------------------------------------------------------------------------

brypt_dispatch_t brypt_service_dispatch_cluster(
    brypt_service_t const* const service, char const* route, uint8_t const* _payload, size_t size) noexcept
{
    brypt_dispatch_t result { 
        .dispatched = 0,
        .result = BRYPT_EINVALIDARGUMENT
    };

    if (brypt_service_t::not_initialized(service)) { return result; }
    if (!route) { return result; }

    result.result = BRYPT_ENOTAVAILABLE;
    try {
        if (auto const spProxyStore = service->get_proxy_store(); spProxyStore) {
            auto const payload = Message::Payload{ std::span{ _payload, size } };
            auto const optResult = spProxyStore->Notify(Message::Destination::Cluster, route, payload);
            if (optResult) {
                result.dispatched = *optResult;
                result.result = BRYPT_ACCEPTED;
            } else {
                result.result = BRYPT_ENOTAVAILABLE;
            }
        }
    }
    catch ([[maybe_unused]] std::bad_alloc const& exception) { result.result = BRYPT_EOUTOFMEMORY; }
    catch (...) { result.result = BRYPT_EUNSPECIFIED; }

    return result;
}

//----------------------------------------------------------------------------------------------------------------------

brypt_dispatch_t brypt_service_dispatch_cluster_sample(
    brypt_service_t const* const service, char const* route, uint8_t const* _payload, size_t size, double sample) noexcept
{
    brypt_dispatch_t result { 
        .dispatched = 0,
        .result = BRYPT_EINVALIDARGUMENT
    };

    if (brypt_service_t::not_initialized(service)) { return result; }
    if (!route) { return result; }
    if (std::isnan(sample) || sample < 0.0 || sample > 1.0) { return result; }

    result.result = BRYPT_ENOTAVAILABLE;
    try {
        if (auto const spProxyStore = service->get_proxy_store(); spProxyStore) {
            std::random_device device;
            std::mt19937 generator{ device() };
            std::bernoulli_distribution distribution{ sample };

            auto const payload = Message::Payload{ std::span{ _payload, size } };
            auto const optResult = spProxyStore->Notify(
                Message::Destination::Cluster, route, payload, 
                [&] (auto const& proxy) { return proxy.IsActive() && distribution(generator); });
            if (optResult) {
                result.dispatched = *optResult;
                result.result = BRYPT_ACCEPTED;
            } else {
                result.result = BRYPT_ENOTAVAILABLE;
            }
        }
    }
    catch ([[maybe_unused]] std::bad_alloc const& exception) { result.result = BRYPT_EOUTOFMEMORY; }
    catch (...) { result.result = BRYPT_EUNSPECIFIED; }

    return result;
}

//----------------------------------------------------------------------------------------------------------------------

brypt_request_t brypt_service_request(
    brypt_service_t const* const service,
    char const* identifier,
    char const* route,
    uint8_t const* _payload,
    size_t size,
    brypt_on_response_t response_callback,
    brypt_on_request_error_t error_callback,
    void* context) noexcept
{
    brypt_request_t result { 
        .key = brypt_request_key_t{ 0, 0 },
        .requested = 0,
        .result = BRYPT_EINVALIDARGUMENT
    };

    if (brypt_service_t::not_initialized(service)) { return result; }
    if (!identifier || !route) { return result; }

    result.result = BRYPT_ENOTAVAILABLE;
    try {
        if (auto const spProxyStore = service->get_proxy_store(); spProxyStore) {
            auto const onResponse = [response_callback, context](Peer::Action::Response const& response) {
                auto const payload = response.GetPayload().GetReadableView();
                brypt_response_t _response{
                    .key = local::translate_tracker_key(response.GetTrackerKey()),
                    .source = static_cast<std::string const&>(response.GetSource()).c_str(),
                    .payload = payload.data(),
                    .payload_size = payload.size(),
                    .status_code = static_cast<brypt_status_code_t>(response.GetStatusCode()),
                    .remaining = response.GetRemaining()
                };
                response_callback(&_response, context);
            };

            auto const onError = [error_callback, context](Peer::Action::Response const& response) {
                auto const payload = response.GetPayload().GetReadableView();
                brypt_response_t _response{
                    .key = local::translate_tracker_key(response.GetTrackerKey()),
                    .source = static_cast<std::string const&>(response.GetSource()).c_str(),
                    .payload = payload.data(),
                    .payload_size = payload.size(),
                    .status_code = static_cast<brypt_status_code_t>(response.GetStatusCode()),
                    .remaining = response.GetRemaining()
                };
                error_callback(&_response, context);
            };

            auto payload = Message::Payload{ std::span{ _payload, size } };
            auto const optTrackerKey = spProxyStore->Request(identifier, route, std::move(payload), onResponse, onError);

            if (optTrackerKey) {
                result.key = local::translate_tracker_key(*optTrackerKey);
                result.requested = 1;
                result.result = BRYPT_ACCEPTED;
            } else {
                result.result = BRYPT_ENOTAVAILABLE;
            }
        }
    }
    catch ([[maybe_unused]] std::bad_alloc const& exception) { result.result = BRYPT_EOUTOFMEMORY; }
    catch (...) { result.result = BRYPT_EUNSPECIFIED; }

    return result;
}

//----------------------------------------------------------------------------------------------------------------------

brypt_request_t brypt_service_request_cluster(
    brypt_service_t const* const service,
    char const* route,
    uint8_t const* _payload,
    size_t size,
    brypt_on_response_t response_callback,
    brypt_on_request_error_t error_callback,
    void* context) noexcept
{
    brypt_request_t result { 
        .key = brypt_request_key_t{ 0, 0 },
        .requested = 0,
        .result = BRYPT_EINVALIDARGUMENT
    };

    if (brypt_service_t::not_initialized(service)) { return result; }
    if (!route) { return result; }
    
    result.result = BRYPT_ENOTAVAILABLE;
    try {
        if (auto const spProxyStore = service->get_proxy_store(); spProxyStore) {
            auto const onResponse = [response_callback, context](Peer::Action::Response const& response) {
                auto const payload = response.GetPayload().GetReadableView();
                brypt_response_t _response{
                    .key = local::translate_tracker_key(response.GetTrackerKey()),
                    .source = static_cast<std::string const&>(response.GetSource()).c_str(),
                    .payload = payload.data(),
                    .payload_size = payload.size(),
                    .status_code = static_cast<brypt_status_code_t>(response.GetStatusCode()),
                    .remaining = response.GetRemaining()
                };
                response_callback(&_response, context);
            };

            auto const onError = [error_callback, context](Peer::Action::Response const& response) {
                auto const payload = response.GetPayload().GetReadableView();
                brypt_response_t _response{
                    .key = local::translate_tracker_key(response.GetTrackerKey()),
                    .source = static_cast<std::string const&>(response.GetSource()).c_str(),
                    .payload = payload.data(),
                    .payload_size = payload.size(),
                    .status_code = static_cast<brypt_status_code_t>(response.GetStatusCode()),
                    .remaining = response.GetRemaining()
                };
                error_callback(&_response, context);
            };

            auto const payload = Message::Payload{ std::span{ _payload, size } };
            auto const optResult = spProxyStore->Request(
                Message::Destination::Cluster, route, payload, onResponse, onError);
            if (optResult) {
                auto const& [key, requested] = *optResult;
                result.key = local::translate_tracker_key(key);
                result.requested = requested;
                result.result = BRYPT_ACCEPTED;
            } else {
                result.result = BRYPT_ENOTAVAILABLE;
            }
        }
    }
    catch ([[maybe_unused]] std::bad_alloc const& exception) { result.result = BRYPT_EOUTOFMEMORY; }
    catch (...) { result.result = BRYPT_EUNSPECIFIED; }

    return result;
}

//----------------------------------------------------------------------------------------------------------------------

brypt_request_t brypt_service_request_cluster_sample(
    brypt_service_t const* const service,
    char const* route,
    uint8_t const* _payload,
    size_t size,
    double sample,
    brypt_on_response_t response_callback,
    brypt_on_request_error_t error_callback,
    void* context) noexcept
{
    brypt_request_t result { 
        .key = brypt_request_key_t{ 0, 0 },
        .requested = 0,
        .result = BRYPT_EINVALIDARGUMENT
    };

    if (brypt_service_t::not_initialized(service)) { return result; }
    if (!route) { return result; }
    if (std::isnan(sample) || sample < 0.0 || sample > 1.0) { return result; }
    
    result.result = BRYPT_ENOTAVAILABLE;
    try {
        if (auto const spProxyStore = service->get_proxy_store(); spProxyStore) {
            auto const onResponse = [response_callback, context] (Peer::Action::Response const& response) {
                auto const payload = response.GetPayload().GetReadableView();
                brypt_response_t _response{
                    .key = local::translate_tracker_key(response.GetTrackerKey()),
                    .source = static_cast<std::string const&>(response.GetSource()).c_str(),
                    .payload = payload.data(),
                    .payload_size = payload.size(),
                    .status_code = static_cast<brypt_status_code_t>(response.GetStatusCode()),
                    .remaining = response.GetRemaining()
                };
                response_callback(&_response, context);
            };

            auto const onError = [error_callback, context] (Peer::Action::Response const& response) {
                auto const payload = response.GetPayload().GetReadableView();
                brypt_response_t _response{
                    .key = local::translate_tracker_key(response.GetTrackerKey()),
                    .source = static_cast<std::string const&>(response.GetSource()).c_str(),
                    .payload = payload.data(),
                    .payload_size = payload.size(),
                    .status_code = static_cast<brypt_status_code_t>(response.GetStatusCode()),
                    .remaining = response.GetRemaining()
                };
                error_callback(&_response, context);
            };

            std::random_device device;
            std::mt19937 generator{ device() };
            std::bernoulli_distribution distribution{ sample };

            auto const payload = Message::Payload{ std::span{ _payload, size } };
            auto const optResult = spProxyStore->Request(
                Message::Destination::Cluster, route, payload, onResponse, onError,
                [&] (auto const& proxy) -> bool { return proxy.IsActive() && distribution(generator); });
            if (optResult) {
                auto const& [key, requested] = *optResult;
                result.key = local::translate_tracker_key(key);
                result.requested = requested;
                result.result = BRYPT_ACCEPTED;
            } else {
                result.result = BRYPT_ENOTAVAILABLE;
            }
        }
    }
    catch ([[maybe_unused]] std::bad_alloc const& exception) { result.result = BRYPT_EOUTOFMEMORY; }
    catch (std::exception const& exception) { std::cout << exception.what() << std::endl; result.result = BRYPT_EUNSPECIFIED; }

    return result;
}

//----------------------------------------------------------------------------------------------------------------------

brypt_result_t brypt_next_respond(brypt_next_key_t const* const key, brypt_next_respond_t const* const options) noexcept
{
    if (!key || !options) { return BRYPT_EINVALIDARGUMENT; }
    if (!options->payload || options->payload_size == 0) { return BRYPT_EINVALIDARGUMENT; }

    try {
        auto const payload = std::span{ options->payload, options->payload_size };
        auto const statusCode = local::translate_status_code(options->status_code);
        bool const success = key->m_next.Respond(payload, statusCode);
        return (success) ? BRYPT_ACCEPTED : BRYPT_EINVALIDARGUMENT;
    }
    catch ([[maybe_unused]] std::bad_alloc const& exception) { return BRYPT_EOUTOFMEMORY; }
    catch (...) { return BRYPT_EUNSPECIFIED; }

    return BRYPT_ENOTAVAILABLE;
}

//----------------------------------------------------------------------------------------------------------------------

brypt_result_t brypt_next_dispatch(brypt_next_key_t const* const key, brypt_next_dispatch_t const* const options) noexcept
{
    if (!key || !options) { return BRYPT_EINVALIDARGUMENT; }
    if (!options->route) { return BRYPT_EINVALIDARGUMENT; }
    if (!options->payload || options->payload_size == 0) { return BRYPT_EINVALIDARGUMENT; }

    try {
        if (!key->m_next.Dispatch(options->route, std::span{ options->payload, options->payload_size })) {
            return BRYPT_EINVALIDARGUMENT;
        }
    }
    catch ([[maybe_unused]] std::bad_alloc const& exception) { return BRYPT_EOUTOFMEMORY; }
    catch (...) { return BRYPT_EUNSPECIFIED; }

    return BRYPT_ACCEPTED; 
}

//----------------------------------------------------------------------------------------------------------------------

brypt_result_t brypt_next_defer(brypt_next_key_t const* const key, brypt_next_defer_t const* const options) noexcept
{
    if (!key || !options) { return BRYPT_EINVALIDARGUMENT; }

    if (!options->notice.route) { return BRYPT_EINVALIDARGUMENT; }
    if (!options->response.payload || options->response.payload_size == 0 ) { return BRYPT_EINVALIDARGUMENT; }

    Message::Destination type = Message::Destination::Invalid;
    switch (options->notice.type) {
        case BRYPT_DESTINATION_CLUSTER: type = Message::Destination::Cluster; break;
        case BRYPT_DESTINATION_NETWORK: type = Message::Destination::Network; break;
        default: break;
    }
    if (type == Message::Destination::Invalid) { return BRYPT_EINVALIDARGUMENT; }
    
    try {
        auto const optTrackerKey = key->m_next.Defer({
            .notice = {
                .type = type,
                .route = options->notice.route,
                .payload = (options->notice.payload) ? 
                    std::span{ options->notice.payload, options->notice.payload_size } : Message::Payload{},
            },
            .response = {
                .payload = std::span{ options->response.payload, options->response.payload_size }
            }
        });

        return (optTrackerKey) ? BRYPT_ACCEPTED : BRYPT_EINVALIDARGUMENT;
    }
    catch ([[maybe_unused]] std::bad_alloc const& exception) { return BRYPT_EOUTOFMEMORY; }
    catch (...) { return BRYPT_EUNSPECIFIED; }
}

//----------------------------------------------------------------------------------------------------------------------

brypt_result_t brypt_event_subscribe_binding_failed(
    brypt_service_t* const service, brypt_event_binding_failed_t callback, void* context) noexcept
{
    if (brypt_service_t::not_created(service)) { return BRYPT_EINVALIDARGUMENT; }

    try {
        if (auto const spPublisher = service->get_publisher(); spPublisher) {
            bool const result = spPublisher->Subscribe<Event::Type::BindingFailed>(
                [callback, context] ([[maybe_unused]] auto identifier, auto const& binding, auto cause) {
                    auto const protocol = local::translate_protocol(binding.GetProtocol());
                    auto const failure = local::translate_binding_failure(cause);
                    callback(protocol, binding.GetUri().data(), failure, context);
                });

            return result ? BRYPT_ACCEPTED : BRYPT_EALREADYSTARTED;
        }
    }
    catch ([[maybe_unused]] std::bad_alloc const& exception) { return BRYPT_EOUTOFMEMORY; }
    catch (...) { return BRYPT_EUNSPECIFIED; }

    return BRYPT_ENOTAVAILABLE;
}

//----------------------------------------------------------------------------------------------------------------------

brypt_result_t brypt_event_subscribe_connection_failed(
    brypt_service_t* const service, brypt_event_connection_failed_t callback, void* context) noexcept
{
    if (brypt_service_t::not_created(service)) { return BRYPT_EINVALIDARGUMENT; }

    try {
        if (auto const spPublisher = service->get_publisher(); spPublisher) {
            bool const result = spPublisher->Subscribe<Event::Type::ConnectionFailed>(
                [callback, context] ([[maybe_unused]] auto identifier, auto const& address, auto cause) {
                    if (address.GetOrigin() == Network::RemoteAddress::Origin::User) {
                        auto const protocol = local::translate_protocol(address.GetProtocol());
                        auto const failure = local::translate_connection_failure(cause);
                        callback(protocol, address.GetUri().data(), failure, context);
                    }
                });

            return result ? BRYPT_ACCEPTED : BRYPT_EALREADYSTARTED;
        }
    }
    catch ([[maybe_unused]] std::bad_alloc const& exception) { return BRYPT_EOUTOFMEMORY; }
    catch (...) { return BRYPT_EUNSPECIFIED; }

    return BRYPT_ENOTAVAILABLE;
}

//----------------------------------------------------------------------------------------------------------------------

brypt_result_t brypt_event_subscribe_endpoint_started(
    brypt_service_t* const service, brypt_event_endpoint_started_t callback, void* context) noexcept
{
    if (brypt_service_t::not_created(service)) { return BRYPT_EINVALIDARGUMENT; }
    
    try {
        if (auto const spPublisher = service->get_publisher(); spPublisher) {
            bool const result = spPublisher->Subscribe<Event::Type::EndpointStarted>(
                [service, callback, context] (auto, auto const& binding) {
                    assert(service);
                    if (auto const& spNetworkManager = service->get_network_manager()) {
                        callback(local::translate_protocol(binding.GetProtocol()), binding.GetUri().data(), context);
                    }
                });

            return result ? BRYPT_ACCEPTED : BRYPT_EALREADYSTARTED;
        }
    }
    catch ([[maybe_unused]] std::bad_alloc const& exception) { return BRYPT_EOUTOFMEMORY; }
    catch (...) { return BRYPT_EUNSPECIFIED; }
    return BRYPT_ENOTAVAILABLE;
}

//----------------------------------------------------------------------------------------------------------------------

brypt_result_t brypt_event_subscribe_endpoint_stopped(
    brypt_service_t* const service, brypt_event_endpoint_stopped_t callback, void* context) noexcept
{
    if (brypt_service_t::not_created(service)) { return BRYPT_EINVALIDARGUMENT; }
    
    try {
        if (auto const spPublisher = service->get_publisher(); spPublisher) {
            bool const result = spPublisher->Subscribe<Event::Type::EndpointStopped>(
                [service, callback, context] (auto, auto const& binding, auto cause) {
                    assert(service);
                    if (auto const& spNetworkManager = service->get_network_manager()) {
                        brypt_result_t result = BRYPT_ACCEPTED;

                        using enum Event::Message<Event::Type::EndpointStopped>::Cause;
                        switch (cause) {
                            case BindingFailed: result = BRYPT_EBINDINGFAILED; break;
                            case ShutdownRequest: result = BRYPT_ESHUTDOWNREQUESTED; break;
                            case UnexpectedError: result = BRYPT_EUNSPECIFIED; break;
                            default: assert(false); break;
                        }

                        callback(
                            local::translate_protocol(binding.GetProtocol()), binding.GetUri().data(), result, context);
                    }
                });

            return result ? BRYPT_ACCEPTED : BRYPT_EALREADYSTARTED;
        }
    }
    catch ([[maybe_unused]] std::bad_alloc const& exception) { return BRYPT_EOUTOFMEMORY; }
    catch (...) { return BRYPT_EUNSPECIFIED; }

    return BRYPT_ENOTAVAILABLE;
}

//----------------------------------------------------------------------------------------------------------------------

brypt_result_t brypt_event_subscribe_peer_connected(
    brypt_service_t* const service, brypt_event_peer_connected_t callback, void* context) noexcept
{
    if (brypt_service_t::not_created(service)) { return BRYPT_EINVALIDARGUMENT; }

    try {
        if (auto const spPublisher = service->get_publisher(); spPublisher) {
            bool const result = spPublisher->Subscribe<Event::Type::PeerConnected>(
                [callback, context] (std::weak_ptr<Peer::Proxy> const& wpProxy, auto const& address) {
                    if (auto const spProxy = wpProxy.lock(); spProxy) {
                        char const* identifier = spProxy->GetIdentifier<Node::External::Identifier>().c_str();
                        callback(identifier, local::translate_protocol(address.GetProtocol()), context);
                    }
                });

            return result ? BRYPT_ACCEPTED : BRYPT_EALREADYSTARTED;
        }
    }
    catch ([[maybe_unused]] std::bad_alloc const& exception) { return BRYPT_EOUTOFMEMORY; }
    catch (...) { return BRYPT_EUNSPECIFIED; }

    return BRYPT_ENOTAVAILABLE;
}

//----------------------------------------------------------------------------------------------------------------------

brypt_result_t brypt_event_subscribe_peer_disconnected(
    brypt_service_t* const service, brypt_event_peer_disconnected_t callback, void* context) noexcept
{
    if (brypt_service_t::not_created(service)) { return BRYPT_EINVALIDARGUMENT; }

    try {
        if (auto const spPublisher = service->get_publisher(); spPublisher) {
            bool const result = spPublisher->Subscribe<Event::Type::PeerDisconnected>(
                [callback, context] (std::weak_ptr<Peer::Proxy> const& wpProxy, auto const& address, auto cause) {
                    if (auto const spProxy = wpProxy.lock(); spProxy) {
                        brypt_result_t result = BRYPT_ACCEPTED;
                        
                        using enum Event::Message<Event::Type::PeerDisconnected>::Cause;
                        switch (cause) {
                            case DisconnectRequest: result = BRYPT_ESHUTDOWNREQUESTED; break;
                            case SessionClosure: result = BRYPT_ESESSIONCLOSED; break;
                            case NetworkShutdown: result = BRYPT_ESHUTDOWNREQUESTED; break;
                            case UnexpectedError: result = BRYPT_EUNSPECIFIED; break;
                            default: assert(false); break;
                        }

                        char const* identifier = spProxy->GetIdentifier<Node::External::Identifier>().c_str();
                        callback(identifier, local::translate_protocol(address.GetProtocol()), result, context);
                    }
                });
                
            return result ? BRYPT_ACCEPTED : BRYPT_EALREADYSTARTED;
        }
    }
    catch ([[maybe_unused]] std::bad_alloc const& exception) { return BRYPT_EOUTOFMEMORY; }
    catch (...) { return BRYPT_EUNSPECIFIED; }

    return BRYPT_ENOTAVAILABLE;
}

//----------------------------------------------------------------------------------------------------------------------

brypt_result_t brypt_event_subscribe_runtime_started(
    brypt_service_t* const service, brypt_event_runtime_started_t callback, void* context) noexcept
{
    if (brypt_service_t::not_created(service)) { return BRYPT_EINVALIDARGUMENT; }

    try {
        if (auto const spPublisher = service->get_publisher(); spPublisher) {
            bool const result = spPublisher->Subscribe<Event::Type::RuntimeStarted>([callback, context] () {
                callback(context);
            });

            return result ? BRYPT_ACCEPTED : BRYPT_EALREADYSTARTED;
        }
    }
    catch ([[maybe_unused]] std::bad_alloc const& exception) { return BRYPT_EOUTOFMEMORY; }
    catch (...) { return BRYPT_EUNSPECIFIED; }

    return BRYPT_ENOTAVAILABLE;
}

//---------------------------------------------------------------------------------------------------------------------- 

brypt_result_t brypt_event_subscribe_runtime_stopped(
    brypt_service_t* const service, brypt_event_runtime_stopped_t callback, void* context) noexcept
{
    if (brypt_service_t::not_created(service)) { return BRYPT_EINVALIDARGUMENT; }

    try {
        if (auto const spPublisher = service->get_publisher(); spPublisher) {
            bool const result = spPublisher->Subscribe<Event::Type::RuntimeStopped>([callback, context] (auto cause) {
                brypt_result_t result = BRYPT_ACCEPTED;

                using enum Event::Message<Event::Type::RuntimeStopped>::Cause;
                switch (cause) {
                    case ShutdownRequest: result = BRYPT_ESHUTDOWNREQUESTED; break;
                    case UnexpectedError: result = BRYPT_EUNSPECIFIED; break;
                    default: assert(false); break;
                }

                callback(result, context);
            });

            return result ? BRYPT_ACCEPTED : BRYPT_EALREADYSTARTED;
        }
    }
    catch ([[maybe_unused]] std::bad_alloc const& exception) { return BRYPT_EOUTOFMEMORY; }
    catch (...) { return BRYPT_EUNSPECIFIED; }

    return BRYPT_ENOTAVAILABLE;
}

//----------------------------------------------------------------------------------------------------------------------

brypt_result_t brypt_service_register_logger(
    brypt_service_t* const service, brypt_on_log_t callback, void* context) noexcept
{
    if (brypt_service_t::not_created(service)) { return BRYPT_EINVALIDARGUMENT; }
    try {
        service->register_logger(callback, context);
    }
    catch ([[maybe_unused]] std::bad_alloc const& exception) { return BRYPT_EOUTOFMEMORY; }
    catch (...) { return BRYPT_EUNSPECIFIED; }

    return BRYPT_ACCEPTED;
}

//----------------------------------------------------------------------------------------------------------------------

char const* brypt_error_description(brypt_result_t code) noexcept
{
    switch (code) {
        case BRYPT_ACCEPTED: return "";
        case BRYPT_ECANCELED: return "Operation canceled";
        case BRYPT_ESHUTDOWNREQUESTED: return "Shutdown requested";
        case BRYPT_EINVALIDARGUMENT: return "Invalid argument";
        case BRYPT_EACCESSDENIED: return "Access denied";
        case BRYPT_EINPROGRESS: return "Operation already in progress";
        case BRYPT_ETIMEOUT: return "Operation timeout";
        case BRYPT_ECONFLICT: return "Operation conflict";
        case BRYPT_EMISSINGFIELD: return "Missing field";
        case BRYPT_EPAYLOADTOOLARGE: return "Payload too large";
        case BRYPT_EOUTOFMEMORY: return "Out of memory";
        case BRYPT_ENOTFOUND: return "Resource not found";
        case BRYPT_ENOTAVAILABLE: return "Resource not available";
        case BRYPT_ENOTSUPPORTED: return "Operation not supported";
        case BRYPT_EUNSPECIFIED: return "Unspecified error";
        case BRYPT_ENOTIMPLEMENTED: return "Not implemented";
        case BRYPT_EINITFAILURE: return "Service could not be initialized";
        case BRYPT_EALREADYSTARTED: return "Service already started";
        case BRYPT_ENOTSTARTED: return "Service is not active";
        case BRYPT_EFILENOTFOUND: return "File could not be found";
        case BRYPT_EFILENOTSUPPORTED: return "File contains an illegal or unsupported option";
        case BRYPT_EINVALIDCONFIG: return "Configuration has an invalid or missing field";
        case BRYPT_EBINDINGFAILED: return "Endpoint could not bind to the specified address";
        case BRYPT_ECONNECTIONFAILED: return "Endpoint could not connect to the specified address";
        case BRYPT_EINVALIDADDRESS: return "Invalid address";
        case BRYPT_EADDRESSINUSE: return "Address already in use";
        case BRYPT_ENOTCONNECTED: return "Address not connected";
        case BRYPT_EALREADYCONNECTED: return "Address already connected";
        case BRYPT_ECONNECTIONREFUSED: return "Connection refused";
        case BRYPT_ENETWORKDOWN: return "Network down";
        case BRYPT_ENETWORKUNREACHABLE: return "Network unreachable";
        case BRYPT_ENETWORKRESET: return "Connection dropped due to a network reset";
        case BRYPT_ENETWORKPERMISSIONS: return "Network lacks appropriate permissions";
        case BRYPT_ESESSIONCLOSED: return "Session closed by the peer";
        default: return "Unknown error";
    }
}

//----------------------------------------------------------------------------------------------------------------------

char const* brypt_status_code_description(brypt_status_code_t code) noexcept
{
    switch (code) {
        case BRYPT_STATUS_OK: return "Ok";
        case BRYPT_STATUS_CREATED: return "Created";
        case BRYPT_STATUS_ACCEPTED: return "Accepted";
        case BRYPT_STATUS_NO_CONTENT: return "No Content";
        case BRYPT_STATUS_PARTIAL_CONTENT: return "Partial Content";
        case BRYPT_STATUS_MOVED_PERMANENTLY: return "Moved Permanently";
        case BRYPT_STATUS_FOUND: return "Found";
        case BRYPT_STATUS_NOT_MODIFIED: return "Not Modified";
        case BRYPT_STATUS_TEMPORARY_REDIRECT: return "Temporary Redirect";
        case BRYPT_STATUS_PERMANENT_REDIRECT: return "Permanent Redirect";
        case BRYPT_STATUS_BAD_REQUEST: return "Bad Request";
        case BRYPT_STATUS_UNAUTHORIZED: return "Unauthorized";
        case BRYPT_STATUS_FORBIDDEN: return "Forbidden";
        case BRYPT_STATUS_NOT_FOUND: return "Not Found";
        case BRYPT_STATUS_REQUEST_TIMEOUT: return "Request Timeout";
        case BRYPT_STATUS_CONFLICT: return "Conflict";
        case BRYPT_STATUS_PAYLOAD_TOO_LARGE: return "Payload Too Large";
        case BRYPT_STATUS_URI_TOO_LONG: return "URI Too Long";
        case BRYPT_STATUS_IM_A_TEAPOT: return "I'm a Teapot";
        case BRYPT_STATUS_LOCKED: return "Locked";
        case BRYPT_STATUS_UPGRADE_REQUIRED: return "Upgrade Required";
        case BRYPT_STATUS_TOO_MANY_REQUESTS: return "Too Many Requests";
        case BRYPT_STATUS_UNAVAILABLE_FOR_LEGAL_REASONS: return "Unavailable for Legal Reasons";
        case BRYPT_STATUS_INTERNAL_SERVER_ERROR: return "Internal Server Error";
        case BRYPT_STATUS_NOT_IMPLEMENTED: return "Not Implemented";
        case BRYPT_STATUS_SERVICE_UNAVAILABLE: return "Service Unavailable";
        case BRYPT_STATUS_INSUFFICIENT_STORAGE: return "Insufficient Storage";
        case BRYPT_STATUS_LOOP_DETECTED: return "Loop Detected";
        default: return "Unknown";
    }
}

//----------------------------------------------------------------------------------------------------------------------

brypt_confidentiality_level_t local::translate_confidentiality_level(Security::ConfidentialityLevel level)
{
    switch (level) {
        case Security::ConfidentialityLevel::Low: return BRYPT_CONFIDENTIALITY_LEVEL_LOW;
        case Security::ConfidentialityLevel::Medium: return BRYPT_CONFIDENTIALITY_LEVEL_MEDIUM;
        case Security::ConfidentialityLevel::High: return BRYPT_CONFIDENTIALITY_LEVEL_HIGH;
        default: return BRYPT_UNKNOWN;
    }
}

//----------------------------------------------------------------------------------------------------------------------

Security::ConfidentialityLevel local::translate_confidentiality_level(brypt_confidentiality_level_t level)
{
    switch (level) {
        case BRYPT_CONFIDENTIALITY_LEVEL_LOW: return Security::ConfidentialityLevel::Low;
        case BRYPT_CONFIDENTIALITY_LEVEL_MEDIUM: return Security::ConfidentialityLevel::Medium;
        case BRYPT_CONFIDENTIALITY_LEVEL_HIGH: return Security::ConfidentialityLevel::High;
        default: return Security::ConfidentialityLevel::Unknown;
    }
}

//----------------------------------------------------------------------------------------------------------------------

brypt_protocol_t local::translate_protocol(Network::Protocol protocol)
{
    switch (protocol) {
        case Network::Protocol::TCP: return BRYPT_PROTOCOL_TCP;
        default: return BRYPT_UNKNOWN;
    }
}

//----------------------------------------------------------------------------------------------------------------------

Network::Protocol local::translate_protocol(brypt_protocol_t protocol)
{
    switch (protocol) {
        case BRYPT_PROTOCOL_TCP: return Network::Protocol::TCP;
        default: return Network::Protocol::Invalid;
    }
}

//----------------------------------------------------------------------------------------------------------------------

brypt_connection_state_t local::translate_connection_state(Network::Connection::State state)
{
    switch (state) {
        case Network::Connection::State::Connected: return BRYPT_CONNECTED_STATE;
        case Network::Connection::State::Disconnected: return BRYPT_DISCONNECTED_STATE;
        default: return BRYPT_UNKNOWN;
    }
}

//----------------------------------------------------------------------------------------------------------------------

brypt_authorization_state_t local::translate_authorization_state(Security::State state)
{
    switch (state) {
        case Security::State::Authorized: return BRYPT_AUTHORIZED_STATE;
        case Security::State::Unauthorized: return BRYPT_UNAUTHORIZED_STATE;
        case Security::State::Flagged: return BRYPT_FLAGGED_STATE;
        default: return BRYPT_UNKNOWN;
    }
}

//----------------------------------------------------------------------------------------------------------------------

brypt_result_t local::translate_binding_failure(BindingFailure failure)
{
    using enum BindingFailure;
    switch (failure) {
        case Canceled: return BRYPT_ECANCELED;
        case AddressInUse: return BRYPT_EADDRESSINUSE;
        case Offline: return BRYPT_ENETWORKDOWN;
        case Unreachable: return BRYPT_ENETWORKUNREACHABLE;
        case Permissions: return BRYPT_ENETWORKPERMISSIONS;
        default: return BRYPT_EUNSPECIFIED;
    }
}

//----------------------------------------------------------------------------------------------------------------------

brypt_result_t local::translate_connection_failure(ConnectionFailure failure)
{
    using enum ConnectionFailure;
    switch (failure) {
        case Canceled: return BRYPT_ECANCELED;
        case InProgress: return BRYPT_EINPROGRESS;
        case Duplicate: return BRYPT_EALREADYCONNECTED;
        case Reflective: return BRYPT_ENOTSUPPORTED;
        case Offline: return BRYPT_ENETWORKDOWN;
        case Unreachable: return BRYPT_ENETWORKUNREACHABLE;
        case Permissions: return BRYPT_ENETWORKPERMISSIONS;
        default: return BRYPT_EUNSPECIFIED;
    }
}

//----------------------------------------------------------------------------------------------------------------------

brypt_request_key_t local::translate_tracker_key(Awaitable::TrackerKey const& key)
{
    brypt_request_key_t _key{ 0, 0 };
    constexpr std::size_t midpoint = sizeof(_key.high);
    std::copy_n(key.begin(), sizeof(_key.high), reinterpret_cast<std::uint8_t*>(&_key.high));
    std::copy_n(key.begin() + midpoint, sizeof(_key.low), reinterpret_cast<std::uint8_t*>(&_key.low));
    return _key;
}

//----------------------------------------------------------------------------------------------------------------------

brypt_status_code_t local::translate_status_code(Message::Extension::Status::Code code)
{
    using enum Message::Extension::Status::Code;
    switch (code) {
        case Ok: return BRYPT_STATUS_OK;
        case Created: return BRYPT_STATUS_CREATED;
        case Accepted: return BRYPT_STATUS_ACCEPTED;
        case NoContent: return BRYPT_STATUS_NO_CONTENT;
        case PartialContent: return BRYPT_STATUS_PARTIAL_CONTENT;
        case MovedPermanently: return BRYPT_STATUS_MOVED_PERMANENTLY;
        case Found: return BRYPT_STATUS_FOUND;
        case NotModified: return BRYPT_STATUS_NOT_MODIFIED;
        case TemporaryRedirect: return BRYPT_STATUS_TEMPORARY_REDIRECT;
        case PermanentRedirect: return BRYPT_STATUS_PERMANENT_REDIRECT;
        case BadRequest: return BRYPT_STATUS_BAD_REQUEST;
        case Unauthorized: return BRYPT_STATUS_UNAUTHORIZED;
        case Forbidden: return BRYPT_STATUS_FORBIDDEN;
        case NotFound: return BRYPT_STATUS_NOT_FOUND;
        case RequestTimeout: return BRYPT_STATUS_REQUEST_TIMEOUT;
        case Conflict: return BRYPT_STATUS_CONFLICT;
        case PayloadTooLarge: return BRYPT_STATUS_PAYLOAD_TOO_LARGE;
        case UriTooLong: return BRYPT_STATUS_URI_TOO_LONG;
        case ImATeapot: return BRYPT_STATUS_IM_A_TEAPOT;
        case Locked: return BRYPT_STATUS_LOCKED;
        case UpgradeRequired: return BRYPT_STATUS_UPGRADE_REQUIRED;
        case TooManyRequests: return BRYPT_STATUS_TOO_MANY_REQUESTS;
        case UnavailableForLegalReasons: return BRYPT_STATUS_UNAVAILABLE_FOR_LEGAL_REASONS;
        case InternalServerError: return BRYPT_STATUS_INTERNAL_SERVER_ERROR;
        case NotImplemented: return BRYPT_STATUS_NOT_IMPLEMENTED;
        case ServiceUnavailable: return BRYPT_STATUS_SERVICE_UNAVAILABLE;
        case InsufficientStorage: return BRYPT_STATUS_INSUFFICIENT_STORAGE;
        case LoopDetected: return BRYPT_STATUS_LOOP_DETECTED;
        default: return BRYPT_UNKNOWN;
    }
}

//----------------------------------------------------------------------------------------------------------------------

Message::Extension::Status::Code local::translate_status_code(brypt_status_code_t code)
{
    using enum Message::Extension::Status::Code;
    switch (code) {
        case BRYPT_STATUS_OK: return Ok;
        case BRYPT_STATUS_CREATED: return Created;
        case BRYPT_STATUS_ACCEPTED: return Accepted;
        case BRYPT_STATUS_NO_CONTENT: return NoContent;
        case BRYPT_STATUS_PARTIAL_CONTENT: return PartialContent;
        case BRYPT_STATUS_MOVED_PERMANENTLY: return MovedPermanently;
        case BRYPT_STATUS_FOUND: return Found;
        case BRYPT_STATUS_NOT_MODIFIED: return NotModified;
        case BRYPT_STATUS_TEMPORARY_REDIRECT: return TemporaryRedirect;
        case BRYPT_STATUS_PERMANENT_REDIRECT: return PermanentRedirect;
        case BRYPT_STATUS_BAD_REQUEST: return BadRequest;
        case BRYPT_STATUS_UNAUTHORIZED: return Unauthorized;
        case BRYPT_STATUS_FORBIDDEN: return Forbidden;
        case BRYPT_STATUS_NOT_FOUND: return NotFound;
        case BRYPT_STATUS_REQUEST_TIMEOUT: return RequestTimeout;
        case BRYPT_STATUS_CONFLICT: return Conflict;
        case BRYPT_STATUS_PAYLOAD_TOO_LARGE: return PayloadTooLarge;
        case BRYPT_STATUS_URI_TOO_LONG: return UriTooLong;
        case BRYPT_STATUS_IM_A_TEAPOT: return ImATeapot;
        case BRYPT_STATUS_LOCKED: return Locked;
        case BRYPT_STATUS_UPGRADE_REQUIRED: return UpgradeRequired;
        case BRYPT_STATUS_TOO_MANY_REQUESTS: return TooManyRequests;
        case BRYPT_STATUS_UNAVAILABLE_FOR_LEGAL_REASONS: return UnavailableForLegalReasons;
        case BRYPT_STATUS_INTERNAL_SERVER_ERROR: return InternalServerError;
        case BRYPT_STATUS_NOT_IMPLEMENTED: return NotImplemented;
        case BRYPT_STATUS_SERVICE_UNAVAILABLE: return ServiceUnavailable;
        case BRYPT_STATUS_INSUFFICIENT_STORAGE: return InsufficientStorage;
        case BRYPT_STATUS_LOOP_DETECTED: return LoopDetected;
        default: return Unknown;
    }
}

//----------------------------------------------------------------------------------------------------------------------
