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
    m_instance.m_spEventPublisher->PublishEvents();
    
    std::this_thread::sleep_for(local::CycleTimeout);
}

//----------------------------------------------------------------------------------------------------------------------

ForegroundRuntime::ForegroundRuntime(BryptNode& instance)
    : IRuntimePolicy(instance)
    , m_active(false)
{
}

//----------------------------------------------------------------------------------------------------------------------

bool ForegroundRuntime::Start()
{
    if (m_active) [[unlikely]] { return false; }

    m_active = true;
    while (m_active) { IRuntimePolicy::ProcessEvents(); }

    return true;
}

//----------------------------------------------------------------------------------------------------------------------

bool ForegroundRuntime::Stop()
{
    m_active = false;
    return true;
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

bool BackgroundRuntime::Start()
{
    if (m_active) [[unlikely]] { return false; }

    m_active = true;
    m_worker = std::jthread([&] (std::stop_token token)
        {
            while (!token.stop_requested()) { IRuntimePolicy::ProcessEvents(); }
        });

    return true;
}

//----------------------------------------------------------------------------------------------------------------------

bool BackgroundRuntime::Stop()
{
    if (!m_active) { return true; }

    m_worker.request_stop();
    if (m_worker.joinable()) { m_worker.join(); }
    
    m_active = false;

    return true;
}

//----------------------------------------------------------------------------------------------------------------------

bool BackgroundRuntime::IsActive() const
{
    return m_active;
}

//----------------------------------------------------------------------------------------------------------------------
