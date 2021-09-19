//----------------------------------------------------------------------------------------------------------------------
// File: BryptNode.hpp
// Description:
//----------------------------------------------------------------------------------------------------------------------
#pragma once
//----------------------------------------------------------------------------------------------------------------------
#include "ExecutionToken.hpp"
#include "RuntimeContext.hpp"
#include "RuntimePolicy.hpp"
#include "BryptIdentifier/IdentifierTypes.hpp"
#include "Components/Configuration/Options.hpp"
#include "Components/Handler/Handler.hpp"
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

namespace Await {
    class ResponseTracker;
    class TrackingManager;
}

namespace Configuration { class Parser; }
namespace Event { class Publisher; }
namespace Network { class Manager; }
namespace Peer { class Manager; }
namespace Scheduler { class Registrar; class TaskService; }

class AuthorizedProcessor;
class BootstrapService;

class CoordinatorState;
class NetworkState;
class NodeState;
class SecurityState;
class SensorState;

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

    Core(
        std::reference_wrapper<ExecutionToken> const& token,
        std::unique_ptr<Configuration::Parser> const& upParser,
        std::shared_ptr<BootstrapService> const& spBootstrapService);

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
    [[nodiscard]] std::weak_ptr<SecurityState> GetSecurityState() const;
    [[nodiscard]] std::weak_ptr<SensorState> GetSensorState() const;

    [[nodiscard]] std::weak_ptr<Event::Publisher> GetEventPublisher() const;
    [[nodiscard]] std::weak_ptr<Network::Manager> GetNetworkManager() const;
    [[nodiscard]] std::weak_ptr<Peer::Manager> GetPeerManager() const;
    [[nodiscard]] std::weak_ptr<BootstrapService> GetBootstrapService() const;
    [[nodiscard]] std::weak_ptr<Await::TrackingManager> GetAwaitManager() const;

private:
    void CreateStaticResources();

    [[nodiscard]] ExecutionStatus StartComponents();
    void OnRuntimeStopped(ExecutionStatus status);
    void OnUnexpectedError();

    std::reference_wrapper<ExecutionToken> m_token;
    std::shared_ptr<Scheduler::Registrar> m_spScheduler;
    std::unique_ptr<IRuntimePolicy> m_upRuntime;
    std::shared_ptr<spdlog::logger> m_logger;

    std::shared_ptr<NodeState> m_spNodeState;
    std::shared_ptr<CoordinatorState> m_spCoordinatorState;
    std::shared_ptr<NetworkState> m_spNetworkState;
    std::shared_ptr<SecurityState> m_spSecurityState;
    std::shared_ptr<SensorState> m_spSensorState;

    std::shared_ptr<Scheduler::TaskService> m_spTaskService;
    std::shared_ptr<Event::Publisher> m_spEventPublisher;
    std::shared_ptr<Network::Manager> m_spNetworkManager;
    std::shared_ptr<Peer::Manager> m_spPeerManager;
    std::shared_ptr<AuthorizedProcessor> m_spMessageProcessor;
    std::shared_ptr<Await::TrackingManager> m_spAwaitManager;
    std::shared_ptr<BootstrapService> m_spBootstrapService;

    Handler::Map m_handlers;
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
        // a spawned thread, it has fully completed exuction. Given we have already returned if the runtime is 
        // in an executing state, the runtime is no longer needed and a subsequent call to start should be possible. 
        m_upRuntime.reset();
    }

    return result;
}

//----------------------------------------------------------------------------------------------------------------------
