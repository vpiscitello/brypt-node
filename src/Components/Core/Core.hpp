//----------------------------------------------------------------------------------------------------------------------
// File: Core.hpp
// Description:
//----------------------------------------------------------------------------------------------------------------------
#pragma once
//----------------------------------------------------------------------------------------------------------------------
#include "ExecutionToken.hpp"
#include "RuntimeContext.hpp"
#include "RuntimePolicy.hpp"
#include "ServiceProvider.hpp"
#include "Components/Configuration/Options.hpp"
#include "Components/Identifier/IdentifierTypes.hpp"
#include "Components/Network/EndpointTypes.hpp"
#include "Components/Peer/Proxy.hpp"
#include "Utilities/ExecutionStatus.hpp"
//----------------------------------------------------------------------------------------------------------------------
#include <concepts>
#include <functional>
#include <memory>
//----------------------------------------------------------------------------------------------------------------------

//----------------------------------------------------------------------------------------------------------------------
// Forward Declarations
//----------------------------------------------------------------------------------------------------------------------

namespace spdlog { class logger; }

namespace Awaitable { class TrackingService; }
namespace Configuration { class Parser; }
namespace Event { class Publisher; }
namespace Network { class Manager; }
namespace Peer { class ProxyStore; }
namespace Route { class Router; }
namespace Route::Fundamental::Connect { class DiscoveryProtocol; }
namespace Scheduler { class Registrar; class TaskService; }
namespace Security { class CipherService; }

class AuthorizedProcessor;
class BootstrapService;

class CoordinatorState;
class NetworkState;
class NodeState;
class SecurityState;

//----------------------------------------------------------------------------------------------------------------------
namespace Node {
//----------------------------------------------------------------------------------------------------------------------

class Core;

//----------------------------------------------------------------------------------------------------------------------
} // Node namespace
//----------------------------------------------------------------------------------------------------------------------

class Node::Core final
{
public:
    explicit Core(std::reference_wrapper<ExecutionToken> const& token);

    Core(Core const& other) = delete;
    Core(Core&& other) = delete;
    Core& operator=(Core const& other) = delete;
    Core& operator=(Core&& other) = delete;

    ~Core();

    // Runtime Handler {
    friend class IRuntimePolicy;
    // } Runtime Handler

    [[nodiscard]] bool IsInitialized() const noexcept;
    [[nodiscard]] bool IsActive() const noexcept;

    [[nodiscard]] bool CreateConfiguredResources(
        std::unique_ptr<Configuration::Parser> const& upParser,
        std::shared_ptr<BootstrapService> const& spBootstrapService);
        
    [[nodiscard]] bool Attach(Configuration::Options::Endpoint const& options);
    [[nodiscard]] bool Detach(Configuration::Options::Endpoint const& options);

    template<ValidRuntimePolicy RuntimePolicy = ForegroundRuntime>
    [[nodiscard]] ExecutionStatus Startup();
    
    ExecutionStatus Shutdown(ExecutionStatus reason = ExecutionStatus::RequestedShutdown);

    [[nodiscard]] std::weak_ptr<NodeState> GetNodeState() const;
    [[nodiscard]] std::weak_ptr<CoordinatorState> GetCoordinatorState() const;
    [[nodiscard]] std::weak_ptr<NetworkState> GetNetworkState() const;

    [[nodiscard]] std::weak_ptr<Event::Publisher> GetEventPublisher() const;
    [[nodiscard]] std::weak_ptr<Route::Router> GetRouter() const;
    [[nodiscard]] std::weak_ptr<Awaitable::TrackingService> GetTrackingService() const;
    [[nodiscard]] std::weak_ptr<Network::Manager> GetNetworkManager() const;
    [[nodiscard]] std::weak_ptr<Peer::ProxyStore> GetProxyStore() const;
    [[nodiscard]] std::weak_ptr<BootstrapService> GetBootstrapService() const;

private:
    void CreateStaticResources();

    [[nodiscard]] ExecutionStatus StartComponents();
    void OnRuntimeStopped(ExecutionStatus status);
    void OnUnexpectedError();

    std::reference_wrapper<ExecutionToken> m_token;
    std::shared_ptr<spdlog::logger> m_logger;
    
    std::shared_ptr<ServiceProvider> m_spServiceProvider;
    std::shared_ptr<Scheduler::Registrar> m_spScheduler;
    std::unique_ptr<IRuntimePolicy> m_upRuntime;

    std::shared_ptr<NodeState> m_spNodeState;
    std::shared_ptr<CoordinatorState> m_spCoordinatorState;
    std::shared_ptr<NetworkState> m_spNetworkState;
    
    std::shared_ptr<Scheduler::TaskService> m_spTaskService;
    std::shared_ptr<Event::Publisher> m_spEventPublisher;
    std::shared_ptr<Security::CipherService> m_spCipherService;
    std::shared_ptr<Route::Router> m_spRouter;
    std::shared_ptr<Awaitable::TrackingService> m_spTrackingService;
    std::shared_ptr<Route::Fundamental::Connect::DiscoveryProtocol> m_spDiscoveryProtocol;
    std::shared_ptr<Network::Manager> m_spNetworkManager;
    std::shared_ptr<Peer::ProxyStore> m_spProxyStore;
    std::shared_ptr<AuthorizedProcessor> m_spMessageProcessor;
    std::shared_ptr<BootstrapService> m_spBootstrapService;

    bool m_initialized;
};

//----------------------------------------------------------------------------------------------------------------------

template<Node::ValidRuntimePolicy RuntimePolicy>
[[nodiscard]] ExecutionStatus Node::Core::Startup()
{
    if (m_token.get().Status() != ExecutionStatus::Standby) { return ExecutionStatus::AlreadyStarted; }
    assert(!m_upRuntime); // Currently, if the token is standby there must not be an existing runtime object. 

    // If we fail to prepare for the execution, return the reason why. 
    if (auto const result = StartComponents(); result != ExecutionStatus::Standby) { return result; }

    m_upRuntime = std::make_unique<RuntimePolicy>(*this, m_token); // Create a new runtime of the provided type. 
    ExecutionStatus result = m_upRuntime->Start(); // Start the execution of the main event loop. 
    if (result != ExecutionStatus::ThreadSpawned) {
        // Note: At this point it's assumed that if the start call returned anything other than a notification of 
        // a spawned thread, it has fully completed execution. Given we have already returned if the runtime is 
        // in an executing state, the runtime is no longer needed and a subsequent call to start should be possible. 
        m_upRuntime.reset();
    }

    return result;
}

//----------------------------------------------------------------------------------------------------------------------
