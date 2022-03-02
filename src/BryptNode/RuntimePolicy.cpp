//----------------------------------------------------------------------------------------------------------------------
// File: RuntimePolicy.cpp
// Description:
//----------------------------------------------------------------------------------------------------------------------
#include "RuntimePolicy.hpp"
//----------------------------------------------------------------------------------------------------------------------
#include "BryptNode.hpp"
#include "Components/Awaitable/TrackingService.hpp"
#include "Components/Configuration/BootstrapService.hpp"
#include "Components/Event/Publisher.hpp"
#include "Components/MessageControl/AuthorizedProcessor.hpp"
#include "Components/Peer/Manager.hpp"
#include "Components/Scheduler/Registrar.hpp"
#include "Utilities/Assertions.hpp"
#include "Utilities/CallbackIteration.hpp"
//----------------------------------------------------------------------------------------------------------------------
#include <spdlog/spdlog.h>
//----------------------------------------------------------------------------------------------------------------------
#include <cassert>
//----------------------------------------------------------------------------------------------------------------------

//----------------------------------------------------------------------------------------------------------------------
namespace {
namespace local {
//----------------------------------------------------------------------------------------------------------------------

constexpr auto WorkTimeout = std::chrono::milliseconds{ 250 };

//----------------------------------------------------------------------------------------------------------------------
} // local namespace
} // namespace
//----------------------------------------------------------------------------------------------------------------------

Node::IRuntimePolicy::IRuntimePolicy(Node::Core& instance, std::reference_wrapper<ExecutionToken> const& token)
    : m_instance(instance)
    , m_token(token)
{
}

//----------------------------------------------------------------------------------------------------------------------

bool Node::IRuntimePolicy::IsExecutionRequested() const
{
    return m_token.get().IsExecutionRequested();
}

//----------------------------------------------------------------------------------------------------------------------

void Node::IRuntimePolicy::SetExecutionStatus(ExecutionStatus status)
{
    m_token.get().SetStatus({}, status);
}

//----------------------------------------------------------------------------------------------------------------------

void Node::IRuntimePolicy::OnExecutionStarted()
{
    assert(Assertions::Threading::RegisterCoreThread()); // Set the core thread to the thread context of the runtime. 
    assert(IsExecutionRequested()); // A start notification should only occur when it has been requested.
    m_instance.m_logger->debug("Starting the node's core runtime.");
    m_token.get().OnExecutionStarted({}); // Put the execution token into the executing state.
}

//----------------------------------------------------------------------------------------------------------------------

ExecutionStatus Node::IRuntimePolicy::OnExecutionStopped() const
{
    // If the cause is not set, it is assumed this shutdown is intentional.
    auto const result = m_token.get().Status();
    m_instance.m_logger->debug("Stopping the node's core runtime.");
    m_token.get().OnExecutionStopped({}); // Put the execution token into the standby state.
    m_instance.OnRuntimeStopped(result); // After this call our resources will be destroyed. 
    return result;
}

//----------------------------------------------------------------------------------------------------------------------

void Node::IRuntimePolicy::ProcessEvents()
{
    // Execute the ready services then wait for the next available task. The timeout is used to ensure a task 
    // notification is not missed from the scheduler. 
    m_instance.m_spScheduler->Execute();
    m_instance.m_spScheduler->AwaitTask(local::WorkTimeout);
    std::this_thread::yield(); // Give other threads an opportunity to run inbetween the execution cycles.  
}

//----------------------------------------------------------------------------------------------------------------------

Node::ForegroundRuntime::ForegroundRuntime(Node::Core& instance, std::reference_wrapper<ExecutionToken> const& token)
    : IRuntimePolicy(instance, token)
{
}

//----------------------------------------------------------------------------------------------------------------------

RuntimeContext Node::ForegroundRuntime::Type() const
{
    return RuntimeContext::Foreground;
}

//----------------------------------------------------------------------------------------------------------------------

ExecutionStatus Node::ForegroundRuntime::Start()
{
    // Note: The foreground runtime can be stopped by another thread or an event due to an unrecoverable error
    // condition. That handler should set the cause to one of the error result values. 
    IRuntimePolicy::OnExecutionStarted();
    while (IRuntimePolicy::IsExecutionRequested()) { IRuntimePolicy::ProcessEvents(); }
    return IRuntimePolicy::OnExecutionStopped();
}

//----------------------------------------------------------------------------------------------------------------------

Node::BackgroundRuntime::BackgroundRuntime(Node::Core& instance, std::reference_wrapper<ExecutionToken> const& token)
    : IRuntimePolicy(instance, token)
    , m_worker()
{
}

//----------------------------------------------------------------------------------------------------------------------

RuntimeContext Node::BackgroundRuntime::Type() const
{
    return RuntimeContext::Background;
}

//----------------------------------------------------------------------------------------------------------------------

ExecutionStatus Node::BackgroundRuntime::Start()
{
    IRuntimePolicy::SetExecutionStatus(ExecutionStatus::ThreadSpawned);
    m_worker = std::jthread([&]
        {
            IRuntimePolicy::OnExecutionStarted();
            while (IRuntimePolicy::IsExecutionRequested()) { IRuntimePolicy::ProcessEvents(); }
            IRuntimePolicy::OnExecutionStopped();
        });

    // Indicate a thread for the runtime has been spawned. Unlike the foreground runtime, shutdown causes shall be 
    // propogated through the event publisher. 
    return ExecutionStatus::ThreadSpawned;
}

//----------------------------------------------------------------------------------------------------------------------
