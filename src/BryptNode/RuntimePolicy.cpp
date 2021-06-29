//----------------------------------------------------------------------------------------------------------------------
// File: RuntimePolicy.cpp
// Description:
//----------------------------------------------------------------------------------------------------------------------
#include "RuntimePolicy.hpp"
//----------------------------------------------------------------------------------------------------------------------
#include "BryptNode.hpp"
#include "Components/Await/TrackingManager.hpp"
#include "Components/Event/Publisher.hpp"
#include "Components/MessageControl/AuthorizedProcessor.hpp"
#include "Components/Peer/Manager.hpp"
//----------------------------------------------------------------------------------------------------------------------
#include "Utilities/CallbackIteration.hpp"
//----------------------------------------------------------------------------------------------------------------------
#include <cassert>
//----------------------------------------------------------------------------------------------------------------------

//----------------------------------------------------------------------------------------------------------------------
namespace {
namespace local {
//----------------------------------------------------------------------------------------------------------------------

constexpr std::chrono::nanoseconds CycleTimeout = std::chrono::milliseconds(1);

//----------------------------------------------------------------------------------------------------------------------
} // local namespace
} // namespace
//----------------------------------------------------------------------------------------------------------------------

IRuntimePolicy::IRuntimePolicy(BryptNode& instance)
    : m_instance(instance)
{
}

//----------------------------------------------------------------------------------------------------------------------

void IRuntimePolicy::ProcessEvents()
{
    if (auto const optMessage = m_instance.m_spMessageProcessor->GetNextMessage(); optMessage) {
        auto const& handlers = m_instance.m_handlers;
        auto& [spPeerProxy, message] = *optMessage;
        if (auto const itr = handlers.find(message.GetCommand()); itr != handlers.end()) {
            auto const& [type, handler] = *itr;
            handler->HandleMessage(*optMessage);
        }
    }

    m_instance.m_spAwaitManager->ProcessFulfilledRequests();
    m_instance.m_spEventPublisher->Dispatch();
    
    std::this_thread::sleep_for(local::CycleTimeout);
}

//----------------------------------------------------------------------------------------------------------------------

ExecutionResult IRuntimePolicy::FinalizeShutdown() const
{
    // If the cause is not set, it is assumed this shutdown is intentional.
    auto const result = m_instance.m_optShutdownCause.value_or(ExecutionResult::RequestedShutdown);
    m_instance.OnRuntimeStopped(); // After this call our resources will be destroyed. 
    return result;
}

//----------------------------------------------------------------------------------------------------------------------

ForegroundRuntime::ForegroundRuntime(BryptNode& instance)
    : IRuntimePolicy(instance)
    , m_active(false)
{
}

//----------------------------------------------------------------------------------------------------------------------

ExecutionResult ForegroundRuntime::Start()
{
    // Note: The foreground runtime can be stopped by another thread or an event due to an unrecoverable error
    // condition. That handler should set the cause to one of the error result values. 
    assert(!m_active); // Currently, the lifecycle of the runtime only exists for one Start/Stop cycle. 
    m_active = true;
    while (m_active) { IRuntimePolicy::ProcessEvents(); }
    return IRuntimePolicy::FinalizeShutdown();
}

//----------------------------------------------------------------------------------------------------------------------

void ForegroundRuntime::Stop()
{
    m_active = false;
    return;
}

//----------------------------------------------------------------------------------------------------------------------

bool ForegroundRuntime::IsActive() const
{
    return m_active;
}

//----------------------------------------------------------------------------------------------------------------------

BackgroundRuntime::BackgroundRuntime(BryptNode& instance)
    : IRuntimePolicy(instance)
    , m_active(false)
    , m_worker()
{
}

//----------------------------------------------------------------------------------------------------------------------

ExecutionResult BackgroundRuntime::Start()
{
    assert(!m_active); // Currently, the lifecycle of the runtime only exists for one Start/Stop cycle. 
    m_active = true;
    m_worker = std::jthread([&] (std::stop_token token)
        {
            while (!token.stop_requested()) { IRuntimePolicy::ProcessEvents(); }
        });

    // Here we indicate the runtime has been spawned. Unlike the foreground runtime, shutdown causes shall be 
    // propogated entirely through the event system. 
    return ExecutionResult::ThreadSpawned;
}

//----------------------------------------------------------------------------------------------------------------------

void BackgroundRuntime::Stop()
{
    if (!m_active) { return; }

    m_worker.request_stop();
    m_active = false;
    [[maybe_unused]] auto const result = IRuntimePolicy::FinalizeShutdown();
}

//----------------------------------------------------------------------------------------------------------------------

bool BackgroundRuntime::IsActive() const
{
    return m_active;
}

//----------------------------------------------------------------------------------------------------------------------
