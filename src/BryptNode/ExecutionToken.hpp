//----------------------------------------------------------------------------------------------------------------------
// File: ExecutionToken.hpp
// Description: Provides access to the core's execution status and enables atomic shutdown/completion. 
//----------------------------------------------------------------------------------------------------------------------
#pragma once
//----------------------------------------------------------------------------------------------------------------------
#include "Utilities/ExecutionStatus.hpp"
//----------------------------------------------------------------------------------------------------------------------
#include <atomic>
#include <cassert>
#include <cstdint>
//----------------------------------------------------------------------------------------------------------------------

//----------------------------------------------------------------------------------------------------------------------
namespace Node {
//----------------------------------------------------------------------------------------------------------------------

class Core;
class IRuntimePolicy;

class ExecutionToken
{
private:
    // Note: This "key" is used to restrict access of mutating the execution status of the runtime. Outside of a 
    // stop request, only base runtime policy may update the state of execution. In the context of the foreground 
    // runtime, the status passed to the RequestStop() is propogated to the caller of the core's Start() method. 
    class StatusKey { public: friend class IRuntimePolicy; private: StatusKey() = default; };

    // Note: This "key" is used to restrict start requests to the core. Currently, only the core can ensure there 
    // will be a future runtime to handle execution. 
    class StartRequestKey { public: friend class Core; private: StartRequestKey() = default; };

public:
    constexpr ExecutionToken() noexcept 
        : m_execute(false)
        , m_status(ExecutionStatus::Standby)
    {
    }

    [[nodiscard]] ExecutionStatus Status() const { return m_status; }
    [[nodiscard]] bool IsExecutionActive() const { return m_status == ExecutionStatus::Executing; }
    [[nodiscard]] bool IsExecutionRequested() const { return m_execute; }

    [[nodiscard]] bool RequestStart([[maybe_unused]] StartRequestKey key)
    {
        // If the execution status is not in the standby state, a start can not be requested. 
        if (m_status != ExecutionStatus::Standby) { return false; }
        assert(m_execute == false); // In the standby state, the execution flag should not be set. 

        // Note: When this flag is set to true the runtime will know that it should enter the event loop. 
        // Currently, a start must be requested before the runtime is spawned. Otherwise the runtime will 
        // immediately return. 
        m_execute = true;
        return true;
    }

    // Note: Only the states indicating execution completion should be provided when requesting a stop. 
    // Additionally, the shutdown statuses do not imply execution has finished, this is only true for standby.
    [[nodiscard]] bool RequestStop(ExecutionStatus reason = ExecutionStatus::RequestedShutdown)
    {
        // If the status is not in the executing state (e.g. already stopping), a stop can not be requested.
        if (m_status != ExecutionStatus::Executing) { return false; }
        assert(m_execute == true); // In the executing state, the execution flag should be set. 
        m_execute = false; // The runtime will stop processing the event loop when this flag is false. 
        m_status = reason; // Indicate execution is in a cleanup phase. 
        return true;
    }

    void SetStatus([[maybe_unused]] StatusKey key, ExecutionStatus status) { m_status = status; }
    void OnExecutionStarted([[maybe_unused]] StatusKey key) { m_status = ExecutionStatus::Executing; }
    void OnExecutionStopped([[maybe_unused]] StatusKey key) { m_status = ExecutionStatus::Standby; }

private:
    std::atomic_bool m_execute;
    std::atomic<ExecutionStatus> m_status;
};

//----------------------------------------------------------------------------------------------------------------------
} // Node namespace
//----------------------------------------------------------------------------------------------------------------------
