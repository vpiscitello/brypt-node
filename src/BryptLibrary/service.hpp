//----------------------------------------------------------------------------------------------------------------------
// Description: Definition of the opaque interface struct exposed through the shared library. 
// Note: This header should not be exported through the include folder. It should only be used internally to 
// act as an abstraction layer for storing user infornmation and the core instance. 
//----------------------------------------------------------------------------------------------------------------------
#pragma once
//----------------------------------------------------------------------------------------------------------------------
#include "brypt.h"
//----------------------------------------------------------------------------------------------------------------------
#include "BryptNode/ExecutionToken.hpp"
#include "Components/Configuration/Parser.hpp"
#include "Components/Configuration/Options.hpp"
//----------------------------------------------------------------------------------------------------------------------
#include <spdlog/spdlog.h>
#include <spdlog/sinks/base_sink.h>
//----------------------------------------------------------------------------------------------------------------------
#include <chrono>
#include <cstring>
#include <filesystem>
#include <memory>
#include <shared_mutex>
#include <string>
#include <string_view>
//----------------------------------------------------------------------------------------------------------------------

namespace Event { class Publisher; }
namespace Network { class Manager; }
namespace Peer { class ProxyStore; }
namespace Route { class Router; }

class BootstrapService;
class NodeState;

//----------------------------------------------------------------------------------------------------------------------

class passthrough_logger : public spdlog::sinks::base_sink<std::mutex>
{
public:
    passthrough_logger();
    void register_logger(brypt_on_log_t watcher, void* context);

protected:
    void sink_it_(spdlog::details::log_msg const& message) override;
    void flush_() override;

private:
    mutable std::shared_mutex m_mutex;
    brypt_on_log_t m_watcher;
    void* m_context;
};

//----------------------------------------------------------------------------------------------------------------------

struct brypt_service_t
{
public:
    brypt_service_t();
    ~brypt_service_t() = default;

    // Note: The following static methods are mainly used to document what state we are checking before each operation. 
    // Service State Checks {
    [[nodiscard]] static bool not_created(brypt_service_t const* const service) noexcept;
    [[nodiscard]] static bool not_initialized(brypt_service_t const* const service) noexcept;
    [[nodiscard]] static bool active(brypt_service_t const* const service) noexcept;
    // } Service State Checks 

    [[nodiscard]] char const* get_base_path() const noexcept;
    [[nodiscard]] char const* get_configuration_filename() const noexcept;
    [[nodiscard]] char const* get_bootstrap_filename() const noexcept;
    [[nodiscard]] std::int32_t get_core_threads() const noexcept;
    [[nodiscard]] bool get_use_bootstraps() const noexcept;
    [[nodiscard]] std::int32_t get_identifier_type() const noexcept;
    [[nodiscard]] std::string const& get_identifier();
    [[nodiscard]] brypt_strategy_t get_security_strategy() const noexcept;
    [[nodiscard]] std::string const& get_node_name() const noexcept;
    [[nodiscard]] std::string const& get_node_description() const noexcept;
    [[nodiscard]] brypt_log_level_t get_log_level() const noexcept;
    [[nodiscard]] std::int32_t get_connection_retry_limit() const noexcept;
    [[nodiscard]] std::chrono::milliseconds get_connection_timeout() const noexcept;
    [[nodiscard]] std::chrono::milliseconds get_connection_retry_interval() const noexcept;
    [[nodiscard]] std::size_t get_endpoint_count() const noexcept;
    [[nodiscard]] Configuration::Options::Endpoints get_endpoints() const;
    [[nodiscard]] bool configuration_file_exists() const noexcept;
    [[nodiscard]] bool configuration_validated() const noexcept;

    [[nodiscard]] brypt_result_t set_base_path(std::string_view path);
    [[nodiscard]] brypt_result_t set_configuration_filename(std::string_view filename);
    [[nodiscard]] brypt_result_t set_bootstrap_filename(std::string_view filename);
    [[nodiscard]] brypt_result_t set_core_threads(std::int32_t threads) noexcept;
    void set_use_bootstraps(bool value) noexcept;
    [[nodiscard]] brypt_result_t set_identifier_type(brypt_identifier_type_t type) noexcept;
    [[nodiscard]] brypt_result_t set_security_strategy(brypt_strategy_t strategy) noexcept;
    [[nodiscard]] brypt_result_t set_node_name(std::string_view name);
    [[nodiscard]] brypt_result_t set_node_description(std::string_view description);
    [[nodiscard]] brypt_result_t set_log_level(brypt_log_level_t level) noexcept;
    [[nodiscard]] brypt_result_t set_connection_retry_limit(std::int32_t limit) noexcept;
    [[nodiscard]] brypt_result_t set_connection_timeout(std::int32_t timeout) noexcept;
    [[nodiscard]] brypt_result_t set_connection_retry_interval(std::int32_t interval) noexcept;

    [[nodiscard]] brypt_result_t attach_endpoint(Configuration::Options::Endpoint&& options);
    [[nodiscard]] brypt_result_t detach_endpoint(Network::Protocol protocol, std::string_view binding) const;

    void register_logger(brypt_on_log_t callback, void* context) const;

    void create_core();
    [[nodiscard]] brypt_result_t startup();
    void shutdown();

    [[nodiscard]] std::shared_ptr<NodeState> get_node_state() const noexcept;
    [[nodiscard]] std::shared_ptr<Network::Manager> get_network_manager() const noexcept;
    [[nodiscard]] std::shared_ptr<Peer::ProxyStore> get_proxy_store() const noexcept;
    [[nodiscard]] std::shared_ptr<Event::Publisher> get_publisher() const noexcept;
    [[nodiscard]] std::shared_ptr<Route::Router> get_router() const noexcept;

private:
    [[nodiscard]] std::filesystem::path get_configuration_filepath() const;
    [[nodiscard]] std::filesystem::path get_bootstrap_filepath() const;

    [[nodiscard]] brypt_result_t fetch_configuration();
    [[nodiscard]] brypt_result_t fetch_bootstraps();
    [[nodiscard]] brypt_result_t initialize_resources();

    [[nodiscard]] bool validate_filepath(std::filesystem::path const& path) noexcept;
    [[nodiscard]] bool validate_filename(std::filesystem::path const& path, std::string_view extension) noexcept;

    std::string m_filepath;

    std::string m_configuration_filename;
    std::unique_ptr<Configuration::Parser> m_configuration_service;

    std::string m_bootstrap_filename;
    std::shared_ptr<BootstrapService> m_bootstrap_service;

    std::shared_ptr<passthrough_logger> m_passthrough_logger;

    Node::ExecutionToken m_token;
    std::shared_ptr<Node::Core> m_core;
};

//----------------------------------------------------------------------------------------------------------------------
