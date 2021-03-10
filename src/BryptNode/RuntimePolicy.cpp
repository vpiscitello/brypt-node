//------------------------------------------------------------------------------------------------
// File: RuntimePolicy.cpp
// Description:
//------------------------------------------------------------------------------------------------
#include "RuntimePolicy.hpp"
//------------------------------------------------------------------------------------------------
#include "BryptNode.hpp"
#include "Components/Await/TrackingManager.hpp"
#include "Components/MessageControl/AuthorizedProcessor.hpp"
#include "Components/BryptPeer/PeerManager.hpp"
//------------------------------------------------------------------------------------------------
#include "Utilities/CallbackIteration.hpp"
//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
namespace {
namespace local {
//------------------------------------------------------------------------------------------------

constexpr std::chrono::nanoseconds CycleTimeout = std::chrono::milliseconds(1);

//------------------------------------------------------------------------------------------------
} // local namespace
} // namespace
//------------------------------------------------------------------------------------------------

IRuntimePolicy::IRuntimePolicy(BryptNode& instance)
    : m_instance(instance)
{
}

//------------------------------------------------------------------------------------------------

void IRuntimePolicy::ProcessEvents()
{
    if (auto const optMessage = m_instance.m_spMessageProcessor->GetNextMessage(); optMessage) {
        ProcessMessage(*optMessage);
    }

    m_instance.m_spAwaitManager->ProcessFulfilledRequests();

    std::this_thread::sleep_for(local::CycleTimeout);
}

//------------------------------------------------------------------------------------------------

void IRuntimePolicy::ProcessMessage(AssociatedMessage const& associatedMessage)
{
    auto const& handlers = m_instance.m_handlers;
    auto& [spBryptPeer, message] = associatedMessage;
    if (auto const itr = handlers.find(message.GetCommand()); itr != handlers.end()) {
        auto const& [type, handler] = *itr;
        handler->HandleMessage(associatedMessage);
    }
}

//------------------------------------------------------------------------------------------------

ForegroundRuntime::ForegroundRuntime(BryptNode& instance)
    : IRuntimePolicy(instance)
    , m_active(false)
{
}

//------------------------------------------------------------------------------------------------

bool ForegroundRuntime::Start()
{
    if (m_active) [[unlikely]] { return false; }

    m_active = true;
    while (m_active) { IRuntimePolicy::ProcessEvents(); }

    return true;
}

//------------------------------------------------------------------------------------------------

bool ForegroundRuntime::Stop()
{
    m_active = false;
    return true;
}

//------------------------------------------------------------------------------------------------

BackgroundRuntime::BackgroundRuntime(BryptNode& instance)
    : IRuntimePolicy(instance)
    , m_active(false)
    , m_worker()
{
}

//------------------------------------------------------------------------------------------------

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

//------------------------------------------------------------------------------------------------

bool BackgroundRuntime::Stop()
{
    if (!m_active) { return true; }

    m_worker.request_stop();
    if (m_worker.joinable()) { m_worker.join(); }
    
    m_active = false;

    return true;
}

//------------------------------------------------------------------------------------------------
