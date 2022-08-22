//----------------------------------------------------------------------------------------------------------------------
#include "service.hpp"
#include "BryptIdentifier/BryptIdentifier.hpp"
#include "BryptNode/BryptNode.hpp"
#include "BryptNode/RuntimeContext.hpp"
#include "Components/Configuration/Parser.hpp"
#include "Components/Configuration/BootstrapService.hpp"
#include "Components/Network/Manager.hpp"
#include "Components/Network/Endpoint.hpp"
#include "Components/Peer/ProxyStore.hpp"
#include "Components/Route/Router.hpp"
#include "Components/State/NodeState.hpp"
#include "Utilities/Assertions.hpp"
#include "Utilities/ExecutionStatus.hpp"
#include "Utilities/FileUtils.hpp"
#include "Utilities/Logger.hpp"
//----------------------------------------------------------------------------------------------------------------------
#include <spdlog/spdlog.h>
//----------------------------------------------------------------------------------------------------------------------
#include <cstdio>
//----------------------------------------------------------------------------------------------------------------------

//----------------------------------------------------------------------------------------------------------------------
namespace {
namespace local {
//----------------------------------------------------------------------------------------------------------------------

constexpr std::size_t SafeFilenameLimit = 255;

brypt_log_level_t translate_log_level(spdlog::level::level_enum level);
spdlog::level::level_enum translate_log_level(brypt_log_level_t level);

//----------------------------------------------------------------------------------------------------------------------
} // local namespace
//----------------------------------------------------------------------------------------------------------------------
namespace defaults {
//----------------------------------------------------------------------------------------------------------------------

constexpr std::string_view ConfigurationFilename = "brypt.config.json";
constexpr std::string_view BootstrapFilename = "brypt.bootstrap.json";
constexpr RuntimeContext Context = RuntimeContext::Background;
constexpr spdlog::level::level_enum Level = spdlog::level::off;
constexpr bool UseBootstraps = true;
constexpr Configuration::Options::Identifier::Type IdentifierType = Configuration::Options::Identifier::Type::Ephemeral;

//----------------------------------------------------------------------------------------------------------------------
} // defaults namespace
} // namespace
//----------------------------------------------------------------------------------------------------------------------

passthrough_logger::passthrough_logger() 
    : m_mutex()
    , m_watcher(nullptr)
    , m_context(nullptr)
{
}

//----------------------------------------------------------------------------------------------------------------------

void passthrough_logger::register_logger(brypt_on_log_t watcher, void* context)
{
    std::scoped_lock lock{ m_mutex };
    m_watcher = watcher;
    m_context = context;
}

//----------------------------------------------------------------------------------------------------------------------

void passthrough_logger::sink_it_(spdlog::details::log_msg const& message)
{
    std::shared_lock lock{ m_mutex };
    if (m_watcher) {
        spdlog::memory_buf_t formatted;
        spdlog::sinks::base_sink<std::mutex>::formatter_->format(message, formatted);
        m_watcher(local::translate_log_level(message.level), formatted.data(), formatted.size(), m_context);
    }
}

//----------------------------------------------------------------------------------------------------------------------

void passthrough_logger::flush_() { }

//----------------------------------------------------------------------------------------------------------------------

brypt_service_t::brypt_service_t()
    : m_filepath()
    , m_configuration_filename(defaults::ConfigurationFilename)
    , m_configuration_service(std::make_unique<Configuration::Parser>(
        Configuration::Options::Runtime{ 
            .context = defaults::Context,
            .verbosity = defaults::Level,
            .useInteractiveConsole = false,
            .useBootstraps = defaults::UseBootstraps,
            .useFilepathDeduction = false
        }
    ))
    , m_bootstrap_filename(defaults::BootstrapFilename)
    , m_bootstrap_service(std::make_shared<BootstrapService>())
    , m_passthrough_logger(std::make_shared<passthrough_logger>())
    , m_core()
{
    auto const spCore = spdlog::get(Logger::Name::Core.data());
    spCore->sinks().emplace_back(m_passthrough_logger);
    
    auto const spTcp = spdlog::get(Logger::Name::TCP.data());
    spTcp->sinks().emplace_back(m_passthrough_logger);

    // By default, the configuration and bootstrap services should have the filesystem usage disabled. 
    assert(m_configuration_service->FilesystemDisabled() && m_bootstrap_service->FilesystemDisabled());

    // Initialize the identifier value now to set it to a reasonable default. If the configuration file has a 
    // persistent identifier it will be read when the base filepath is set. 
    if (!m_configuration_service->SetNodeIdentifier(defaults::IdentifierType)) {
        throw std::runtime_error("Failed to generate brypt identifier!"); // Force the nothrow allocator to return a nullptr.
    }

    // Set some reasonable defaults for the required configuration fields. 
    m_configuration_service->SetSecurityStrategy(Security::Strategy::PQNISTL3); 
}

//----------------------------------------------------------------------------------------------------------------------

bool brypt_service_t::not_created(brypt_service_t const* const service) noexcept
{
    return !(service && service->m_core);
}

//----------------------------------------------------------------------------------------------------------------------

bool brypt_service_t::not_initialized(brypt_service_t const* const service) noexcept
{
    return !(service && service->m_core && service->m_core->IsInitialized());
}

//----------------------------------------------------------------------------------------------------------------------

bool brypt_service_t::active(brypt_service_t const* const service) noexcept
{
    return service && service->m_core && service->m_core->IsActive();
}

//----------------------------------------------------------------------------------------------------------------------

char const* brypt_service_t::get_base_path() const noexcept { return m_filepath.c_str(); }

//----------------------------------------------------------------------------------------------------------------------

char const* brypt_service_t::get_configuration_filename() const noexcept
{
    assert(m_configuration_service);
    return !m_configuration_service->FilesystemDisabled() ?  m_configuration_filename.c_str() : "";
}

//----------------------------------------------------------------------------------------------------------------------

char const* brypt_service_t::get_bootstrap_filename() const noexcept
{
    assert(m_bootstrap_service);
    return !m_bootstrap_service->FilesystemDisabled() ?  m_bootstrap_filename.c_str() : "";
}

//----------------------------------------------------------------------------------------------------------------------

std::int32_t brypt_service_t::get_core_threads() const noexcept
{
    assert(m_configuration_service);
    switch (m_configuration_service->GetRuntimeContext()) {
        case RuntimeContext::Foreground: return 0;
        case RuntimeContext::Background: return 1;
        default: assert(false); return BRYPT_UNKNOWN;
    }
}

//----------------------------------------------------------------------------------------------------------------------

bool brypt_service_t::get_use_bootstraps() const noexcept {
    assert(m_configuration_service);
    return m_configuration_service->UseBootstraps();
}

//----------------------------------------------------------------------------------------------------------------------

std::int32_t brypt_service_t::get_identifier_type() const noexcept
{
    assert(m_configuration_service);
    using namespace Configuration::Options;
    switch (m_configuration_service->GetIdentifierType()) {
        case Identifier::Type::Ephemeral: return BRYPT_IDENTIFIER_EPHEMERAL;
        case Identifier::Type::Persistent: return BRYPT_IDENTIFIER_PERSISTENT;
        default: assert(false); return BRYPT_UNKNOWN;
    }
}

//----------------------------------------------------------------------------------------------------------------------

std::string const& brypt_service_t::get_identifier() 
{
    assert(m_configuration_service);
    using namespace Configuration::Options;
    auto const& spIdentifier = m_configuration_service->GetNodeIdentifier();
    assert(spIdentifier);
    return static_cast<std::string const&>(*spIdentifier);
}

//----------------------------------------------------------------------------------------------------------------------

brypt_strategy_t brypt_service_t::get_security_strategy() const noexcept
{
    assert(m_configuration_service);
    switch (m_configuration_service->GetSecurityStrategy()) {
        case Security::Strategy::PQNISTL3: return BRYPT_STRATEGY_PQNISTL3;
        default: assert(false); return BRYPT_UNKNOWN;
    }
}

//----------------------------------------------------------------------------------------------------------------------

std::string const& brypt_service_t::get_node_name() const noexcept
{
    assert(m_configuration_service);
    return m_configuration_service->GetNodeName();
}

//----------------------------------------------------------------------------------------------------------------------

std::string const& brypt_service_t::get_node_description() const noexcept
{
    assert(m_configuration_service);
    return m_configuration_service->GetNodeDescription();
}

//----------------------------------------------------------------------------------------------------------------------

brypt_log_level_t brypt_service_t::get_log_level() const noexcept
{
    assert(m_configuration_service);
    return local::translate_log_level(m_configuration_service->GetVerbosity());
}

//----------------------------------------------------------------------------------------------------------------------

std::int32_t brypt_service_t::get_connection_retry_limit() const noexcept
{
    assert(m_configuration_service);
    return m_configuration_service->GetConnectionRetryLimit();
}

//----------------------------------------------------------------------------------------------------------------------

std::chrono::milliseconds brypt_service_t::get_connection_timeout() const noexcept
{
    assert(m_configuration_service);
    return m_configuration_service->GetConnectionTimeout();
}

//----------------------------------------------------------------------------------------------------------------------

std::chrono::milliseconds brypt_service_t::get_connection_retry_interval() const noexcept
{
    assert(m_configuration_service);
    return m_configuration_service->GetConnectionRetryInterval();
}

//----------------------------------------------------------------------------------------------------------------------

std::size_t brypt_service_t::get_endpoint_count() const noexcept
{
    assert(m_configuration_service);
    return m_configuration_service->GetEndpoints().size();
}

//----------------------------------------------------------------------------------------------------------------------

Configuration::Options::Endpoints brypt_service_t::get_endpoints() const
{
    assert(m_configuration_service);
    return m_configuration_service->GetEndpoints();
}

//----------------------------------------------------------------------------------------------------------------------

bool brypt_service_t::configuration_file_exists() const noexcept
{
    assert(m_configuration_service);
    std::error_code error;
    if (!std::filesystem::exists(m_configuration_service->GetFilepath(), error)) { return false; }
    if (error) { return false; }
    return true;
}

//----------------------------------------------------------------------------------------------------------------------

bool brypt_service_t::configuration_validated() const noexcept
{
    assert(m_configuration_service);
    return m_configuration_service->Validated();
}

//----------------------------------------------------------------------------------------------------------------------

brypt_result_t brypt_service_t::set_base_path(std::string_view filepath)
{
    assert(m_configuration_service && m_bootstrap_service);
    if (filepath == m_filepath) { return BRYPT_ACCEPTED; } // If the values match, there is nothing to do.

    m_filepath = filepath;
    bool const allowable = filepath.empty() || validate_filepath(m_filepath);
    if (allowable) { 
        // If the filepath is not empty, we need to ensure the configuration and bootstrap filepaths are set. 
        // Otherise, an empty base path implictly disables both the configuration and bootstrap file usage. 
        if (!m_filepath.empty()) {
            m_configuration_service->SetFilepath(get_configuration_filepath());
            m_bootstrap_service->SetFilepath(get_bootstrap_filepath());
        } else {
            m_configuration_service->DisableFilesystem();
            m_bootstrap_service->DisableFilesystem();
        }
        
        return BRYPT_ACCEPTED;
    }

    m_filepath.clear(); // If the filepath was not validated, clear the stored value. 
    return BRYPT_EINVALIDARGUMENT;
}

//----------------------------------------------------------------------------------------------------------------------

brypt_result_t brypt_service_t::set_configuration_filename(std::string_view filename)
{
    assert(m_configuration_service);
    constexpr std::string_view RequiredFileExtension = ".json";
    if (filename == m_configuration_filename) { return BRYPT_ACCEPTED; } // If the values match, there is nothing to do.

    brypt_result_t result = BRYPT_ACCEPTED;
    m_configuration_filename = filename; // Greedily convert the provided filename.
    bool const allowable = filename.empty() || validate_filename(m_configuration_filename, RequiredFileExtension);
    if (!allowable) { result = BRYPT_EINVALIDARGUMENT; }

    // If the provided value points to a potentially fetchable file, set the service's filepath and pre-fetch the
    // configuration if the file exists. If the pre-fetch fails, fallthrough to clear the set filepath. 
    if (bool const fetchable = allowable && !m_filepath.empty() && !m_configuration_filename.empty(); fetchable) {
        m_configuration_service->SetFilepath(get_configuration_filepath());
        if (!std::filesystem::exists(m_configuration_service->GetFilepath())) { return result; }
        if (result = fetch_configuration(); result == BRYPT_ACCEPTED) { return result; }
    }

    m_configuration_service->DisableFilesystem(); 
    m_configuration_filename.clear();

    return result;
}

//----------------------------------------------------------------------------------------------------------------------

brypt_result_t brypt_service_t::set_bootstrap_filename(std::string_view filename)
{
    assert(m_bootstrap_service);
    constexpr std::string_view RequiredFileExtension = ".json";
    if (filename == m_bootstrap_filename) { return BRYPT_ACCEPTED; } // If the values match, there is nothing to do.

    brypt_result_t result = BRYPT_ACCEPTED;
    m_bootstrap_filename = filename; // Greedily convert the provided filename.
    bool const allowable = filename.empty() || validate_filename(m_bootstrap_filename, RequiredFileExtension);
    if (!allowable) { result = BRYPT_EINVALIDARGUMENT; }

    // If the provided value points to a potentially fetchable file, set the service's filepath and pre-fetch the
    // bootstraps if the file exists. If the pre-fetch fails, fallthrough to clear the set filepath. 
    if (bool const fetchable = allowable && !m_filepath.empty() && !m_bootstrap_filename.empty(); fetchable) {
        m_bootstrap_service->SetFilepath(get_bootstrap_filepath());
        if (!std::filesystem::exists(m_bootstrap_service->GetFilepath())) { return result; }
        if (result = fetch_bootstraps(); result == BRYPT_ACCEPTED) { return result; }
    }

    m_bootstrap_service->DisableFilesystem(); 
    m_bootstrap_filename.clear();

    return result;
}

//----------------------------------------------------------------------------------------------------------------------

brypt_result_t brypt_service_t::set_core_threads(std::int32_t threads) noexcept
{
    assert(m_configuration_service);
    if (threads < 0) { return BRYPT_EINVALIDARGUMENT; }
    if (threads > 1) { return BRYPT_ENOTIMPLEMENTED; }
    m_configuration_service->SetRuntimeContext(static_cast<RuntimeContext>(threads));
    return BRYPT_ACCEPTED;
}

//----------------------------------------------------------------------------------------------------------------------

void brypt_service_t::set_use_bootstraps(bool value) noexcept
{
    assert(m_configuration_service);
    m_configuration_service->SetUseBootstraps(value);
}

//----------------------------------------------------------------------------------------------------------------------

brypt_result_t brypt_service_t::set_identifier_type(brypt_identifier_type_t type) noexcept
{
    assert(m_configuration_service);
    using namespace Configuration::Options;
    Identifier::Type translated;
    switch (type) {
        case BRYPT_IDENTIFIER_EPHEMERAL: translated = Identifier::Type::Ephemeral; break;
        case BRYPT_IDENTIFIER_PERSISTENT: translated = Identifier::Type::Persistent; break;
        default: return BRYPT_EINVALIDARGUMENT;
    }
    return m_configuration_service->SetNodeIdentifier(translated) ? BRYPT_ACCEPTED : BRYPT_EUNSPECIFIED;
}

//----------------------------------------------------------------------------------------------------------------------

brypt_result_t brypt_service_t::set_security_strategy(brypt_strategy_t strategy) noexcept
{
    assert(m_configuration_service);
    switch (strategy) {
        case BRYPT_STRATEGY_PQNISTL3: m_configuration_service->SetSecurityStrategy(Security::Strategy::PQNISTL3); break;
        default: return BRYPT_EINVALIDARGUMENT;
    }
    return BRYPT_ACCEPTED;
}

//----------------------------------------------------------------------------------------------------------------------

brypt_result_t brypt_service_t::set_node_name(std::string_view name)
{
    assert(m_configuration_service);
    if (!m_configuration_service->SetNodeName(name)) { return BRYPT_EPAYLOADTOOLARGE; }
    return BRYPT_ACCEPTED;
}

//----------------------------------------------------------------------------------------------------------------------

brypt_result_t brypt_service_t::set_node_description(std::string_view description)
{
    assert(m_configuration_service);
    if (!m_configuration_service->SetNodeDescription(description)) { return BRYPT_EPAYLOADTOOLARGE; }
    return BRYPT_ACCEPTED;
}

//----------------------------------------------------------------------------------------------------------------------

brypt_result_t brypt_service_t::set_log_level(brypt_log_level_t _level) noexcept
{
    assert(m_configuration_service);
    if (_level < BRYPT_LOG_LEVEL_OFF || _level > BRYPT_LOG_LEVEL_CRITICAL) { return BRYPT_EINVALIDARGUMENT; }
    auto const level = local::translate_log_level(_level);
    m_configuration_service->SetVerbosity(level);
    spdlog::set_level(level);
    return BRYPT_ACCEPTED;
}

//----------------------------------------------------------------------------------------------------------------------

brypt_result_t brypt_service_t::set_connection_retry_limit(std::int32_t limit) noexcept
{
    assert(m_configuration_service);
    if (!m_configuration_service->SetConnectionRetryLimit(limit)) { return BRYPT_EINVALIDARGUMENT; }
    return BRYPT_ACCEPTED;
}

//----------------------------------------------------------------------------------------------------------------------

brypt_result_t brypt_service_t::set_connection_timeout(std::int32_t timeout) noexcept
{
    assert(m_configuration_service);
    if (!std::in_range<std::uint32_t>(timeout)) { return BRYPT_EINVALIDARGUMENT; }
    if (!m_configuration_service->SetConnectionTimeout(std::chrono::milliseconds{ timeout })) { 
        return BRYPT_EINVALIDARGUMENT;
    }
    return BRYPT_ACCEPTED;
}

//----------------------------------------------------------------------------------------------------------------------

brypt_result_t brypt_service_t::set_connection_retry_interval(std::int32_t interval) noexcept
{
    assert(m_configuration_service);
    if (!std::in_range<std::uint32_t>(interval)) { return BRYPT_EINVALIDARGUMENT; }
    if (!m_configuration_service->SetConnectionRetryInterval(std::chrono::milliseconds{ interval })) { 
        return BRYPT_EINVALIDARGUMENT;
    }
    return BRYPT_ACCEPTED;
}

//----------------------------------------------------------------------------------------------------------------------


brypt_result_t brypt_service_t::attach_endpoint(Configuration::Options::Endpoint&& options)
{
    using namespace Network;
    assert(m_configuration_service && m_bootstrap_service);
    
    auto const optPriorOptions = m_configuration_service->ExtractEndpoint(
        options.GetProtocol(), options.GetBindingField());
    auto const optStoredOptions = m_configuration_service->UpsertEndpoint(std::move(options));

    // If the provided options could not be initialized and stored return an error. 
    if (!optStoredOptions) { return BRYPT_EINVALIDARGUMENT; } 

    // If we have a core and it has already been initialized, we need to either need to create the endpoint resources
    // or update the existing endpoints. 
    if (m_core && m_core->IsActive()) {
        auto const spNetworkManager = get_network_manager(); // Use the network manager to apply the updates.
        if (!spNetworkManager) { return BRYPT_EUNSPECIFIED; }

        // If we did not match the provided options, then use the core to attach an endpoint and create the resources.
        // Otherwise, we can use the network manager to perform and required updates on the existing endpoints. 
        if (!optPriorOptions) {
            if (!m_core->Attach(optStoredOptions->get())) { return BRYPT_ENOTSUPPORTED; }

            // If the provided options have a bootstrap, scheduele a connect becuase the bootstrap won't contain
            // the configured address when the connections taks is run on the core thread.
            if (auto const optStoredBootstrap = optStoredOptions->get().GetBootstrap(); optStoredBootstrap) { 
                if (!spNetworkManager->ScheduleConnect(*optStoredBootstrap)) {
                    return BRYPT_ECONNECTIONFAILED;
                }
            }
        } else {
            // If the constructed binding has changed (i.e. due to an interface change), schedule the new binding. 
            if (auto const& binding = optStoredOptions->get().GetBinding(); binding != optPriorOptions->GetBinding()) {
                if (!spNetworkManager->ScheduleBind(binding)) {
                    return BRYPT_EBINDINGFAILED;
                }
            }

            // If the provided options have a bootstrap and it has been updated, scheduele a connect. 
            if (auto const optStoredBootstrap = optStoredOptions->get().GetBootstrap(); optStoredBootstrap) {
                auto const optPriorBootstrap = optPriorOptions->GetBootstrap();
                bool const connect = !optPriorBootstrap.has_value() || *optPriorBootstrap != *optStoredBootstrap; 
                if (connect && !spNetworkManager->ScheduleConnect(*optStoredBootstrap)) { 
                    return BRYPT_ECONNECTIONFAILED;
                }
            }
        }
    }

    return BRYPT_ACCEPTED;
}

//----------------------------------------------------------------------------------------------------------------------

brypt_result_t brypt_service_t::detach_endpoint(Network::Protocol protocol, std::string_view binding) const
{
    assert(m_configuration_service);
    auto const optExtractedOptions = m_configuration_service->ExtractEndpoint(protocol, binding);
    if (!optExtractedOptions) { return BRYPT_ENOTFOUND; }
    if (m_core && !m_core->Detach(*optExtractedOptions)) { return BRYPT_ENOTFOUND; }
    return BRYPT_ACCEPTED;
}

//----------------------------------------------------------------------------------------------------------------------

void brypt_service_t::register_logger(brypt_on_log_t callback, void* context) const
{
    assert(m_passthrough_logger);
    m_passthrough_logger->register_logger(callback, context);
}

//----------------------------------------------------------------------------------------------------------------------

void brypt_service_t::create_core()
{
    m_core = std::make_unique<Node::Core>(std::ref(m_token));
}

//----------------------------------------------------------------------------------------------------------------------

brypt_result_t brypt_service_t::startup()
{
    assert(m_core); // Callers should always check if the service has been initialized first. 
    
    if (m_core->IsActive()) { return BRYPT_EALREADYSTARTED; }

    assert(Assertions::Threading::RegisterCoreThread()); // Reset the core thread for startup assertions. 
    if (auto const result = initialize_resources(); result != BRYPT_ACCEPTED) { return result; }

    switch (m_configuration_service->GetRuntimeContext()) {
        case RuntimeContext::Foreground: {
            auto const result = m_core->Startup<Node::ForegroundRuntime>();
            switch (result) {
                case ExecutionStatus::RequestedShutdown: break;
                case ExecutionStatus::InitializationFailed: return BRYPT_EINITFAILURE;
                case ExecutionStatus::UnexpectedShutdown: return BRYPT_EUNSPECIFIED;
                default: assert(false); return BRYPT_EUNSPECIFIED;
            }
        } break;
        case RuntimeContext::Background: {
            auto const result = m_core->Startup<Node::BackgroundRuntime>();
            switch (result) {
                case ExecutionStatus::ThreadSpawned: break;
                default: assert(false); return BRYPT_EUNSPECIFIED;
            }
            assert(Assertions::Threading::WithdrawCoreThread()); // This thread is no longer the core thread.
        } break;
        default: assert(false); return BRYPT_EINITFAILURE;
    }

    return BRYPT_ACCEPTED;
}

//----------------------------------------------------------------------------------------------------------------------

void brypt_service_t::shutdown()
{
    assert(m_core); // Callers should always check if the service has been initialized first. 
    m_core->Shutdown();
}

//----------------------------------------------------------------------------------------------------------------------

std::shared_ptr<NodeState> brypt_service_t::get_node_state() const noexcept
{
    assert(m_core); // Callers should always check if the service has been initialized first. 
    return m_core->GetNodeState().lock();
}

//----------------------------------------------------------------------------------------------------------------------

std::shared_ptr<Network::Manager> brypt_service_t::get_network_manager() const noexcept
{
    assert(m_core); // Callers should always check if the service has been initialized first. 
    return m_core->GetNetworkManager().lock();
}

//----------------------------------------------------------------------------------------------------------------------

std::shared_ptr<Peer::ProxyStore> brypt_service_t::get_proxy_store() const noexcept
{
    assert(m_core); // Callers should always check if the service has been initialized first. 
    return m_core->GetProxyStore().lock();
}

//----------------------------------------------------------------------------------------------------------------------

std::shared_ptr<Event::Publisher> brypt_service_t::get_publisher() const noexcept
{
    assert(m_core); // Callers should always check if the service has been initialized first. 
    return m_core->GetEventPublisher().lock();
}

//----------------------------------------------------------------------------------------------------------------------

std::shared_ptr<Route::Router> brypt_service_t::get_router() const noexcept
{
    assert(m_core); // Callers should always check if the service has been initialized first. 
    return m_core->GetRouter().lock();
}

//----------------------------------------------------------------------------------------------------------------------

std::filesystem::path brypt_service_t::get_configuration_filepath() const
{
    return m_filepath / std::filesystem::path{ m_configuration_filename };
}

//----------------------------------------------------------------------------------------------------------------------

std::filesystem::path brypt_service_t::get_bootstrap_filepath() const
{
    return m_filepath / std::filesystem::path{ m_bootstrap_filename };
}

//----------------------------------------------------------------------------------------------------------------------

brypt_result_t brypt_service_t::fetch_configuration()
{
    assert(m_configuration_service);
    switch (m_configuration_service->FetchOptions()) {
        case Configuration::StatusCode::FileError: return BRYPT_EFILENOTFOUND;
        case Configuration::StatusCode::DecodeError: return BRYPT_EFILENOTSUPPORTED;
        case Configuration::StatusCode::InputError: return BRYPT_EINVALIDCONFIG;
        default: break;
    }
    return BRYPT_ACCEPTED;
}

//----------------------------------------------------------------------------------------------------------------------

brypt_result_t brypt_service_t::fetch_bootstraps()
{
    assert(m_configuration_service && m_bootstrap_service);
    m_bootstrap_service->SetDefaults(m_configuration_service->GetEndpoints());
    if (!m_bootstrap_service->FetchBootstraps()) {
        return (m_bootstrap_service->FilesystemDisabled()) ? BRYPT_EUNSPECIFIED : BRYPT_EFILENOTSUPPORTED;
    }
    return BRYPT_ACCEPTED;
}

//----------------------------------------------------------------------------------------------------------------------

brypt_result_t brypt_service_t::initialize_resources()
{
    assert(m_core); // Callers should always check if the service has been initialized first. 

    if (!m_configuration_service->Validated()) {
        if (auto const result = fetch_configuration(); result != BRYPT_ACCEPTED) { return result; }
    }

    if (auto const result = fetch_bootstraps(); result != BRYPT_ACCEPTED) { return result; }
    if (!m_core->CreateConfiguredResources(m_configuration_service, m_bootstrap_service)) {
        return BRYPT_EINVALIDCONFIG;
    }

    return BRYPT_ACCEPTED;
}

//----------------------------------------------------------------------------------------------------------------------

bool brypt_service_t::validate_filepath(std::filesystem::path const& path) noexcept
{
    #if defined(_WIN32)
    std::size_t const path_size = wcslen(path.c_str());
    bool const excessive = 
        (path_size + m_configuration_filename.size() > local::SafeFilenameLimit) || 
        (path_size + m_bootstrap_filename.size() > local::SafeFilenameLimit);
    #else
    bool const excessive = false;
    #endif

    if (excessive) { return false; }
    
    std::error_code error;
    if (path.has_extension() || !std::filesystem::exists(path, error)) { return false; }
    if (error) { return false; }
    return true;
}

//----------------------------------------------------------------------------------------------------------------------

bool brypt_service_t::validate_filename(std::filesystem::path const& path, std::string_view extension) noexcept
{
    #if defined(_WIN32)
    bool const excessive = strlen(m_filepath.c_str()) + wcslen(path.c_str()) > local::SafeFilenameLimit;
    #else
    bool const excessive = strlen(path.c_str()) > local::SafeFilenameLimit;
    #endif

    if (excessive) { return false; }
    if (path.has_parent_path() || !path.has_filename()) { return false; }
    if (!path.has_extension() || path.extension() != extension) { return false; }
    return true;
}

//----------------------------------------------------------------------------------------------------------------------

brypt_log_level_t local::translate_log_level(spdlog::level::level_enum level)
{
    switch (level) {
        case spdlog::level::off: return BRYPT_LOG_LEVEL_OFF;
        case spdlog::level::trace: return BRYPT_LOG_LEVEL_TRACE; 
        case spdlog::level::debug: return BRYPT_LOG_LEVEL_DEBUG;
        case spdlog::level::info: return BRYPT_LOG_LEVEL_INFO;
        case spdlog::level::warn: return BRYPT_LOG_LEVEL_WARNING;
        case spdlog::level::err: return BRYPT_LOG_LEVEL_ERROR;
        case spdlog::level::critical: return BRYPT_LOG_LEVEL_CRITICAL;
        default: return BRYPT_LOG_LEVEL_OFF;
    }
}

//----------------------------------------------------------------------------------------------------------------------

spdlog::level::level_enum local::translate_log_level(brypt_log_level_t level)
{
    switch (level) {
        case BRYPT_LOG_LEVEL_OFF: return spdlog::level::off;
        case BRYPT_LOG_LEVEL_TRACE: return spdlog::level::trace;
        case BRYPT_LOG_LEVEL_DEBUG: return spdlog::level::debug;
        case BRYPT_LOG_LEVEL_INFO: return spdlog::level::info;
        case BRYPT_LOG_LEVEL_WARNING: return spdlog::level::warn;
        case BRYPT_LOG_LEVEL_ERROR: return spdlog::level::err;
        case BRYPT_LOG_LEVEL_CRITICAL: return spdlog::level::critical;
        default: return spdlog::level::off;
    }
}

//----------------------------------------------------------------------------------------------------------------------
