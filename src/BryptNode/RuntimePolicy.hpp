//----------------------------------------------------------------------------------------------------------------------
// File: RuntimePolicy.hpp
// Description:
//----------------------------------------------------------------------------------------------------------------------
#pragma once
//----------------------------------------------------------------------------------------------------------------------
#include "Components/MessageControl/AssociatedMessage.hpp"
//----------------------------------------------------------------------------------------------------------------------
#include <atomic>
#include <concepts>
#include <memory>
#include <thread>
//----------------------------------------------------------------------------------------------------------------------

class BryptNode;

class IRuntimePolicy
{
public:
    explicit IRuntimePolicy(BryptNode& instance);
    virtual ~IRuntimePolicy() = default;

    IRuntimePolicy(IRuntimePolicy const&) = delete; 
    IRuntimePolicy(IRuntimePolicy&& ) = delete; 
    IRuntimePolicy& operator=(IRuntimePolicy const&) = delete; 
    IRuntimePolicy& operator=(IRuntimePolicy&&) = delete; 

    [[nodiscard]] virtual bool Start() = 0;
    [[nodiscard]] virtual bool Stop() = 0;

protected:
    virtual void ProcessEvents() final;
    virtual void ProcessMessage(AssociatedMessage const& associatedMessage) final;

    BryptNode& m_instance;
};

//----------------------------------------------------------------------------------------------------------------------

class ForegroundRuntime final : public IRuntimePolicy
{
public:
    explicit ForegroundRuntime(BryptNode& instance);

    [[nodiscard]] virtual bool Start() override;
    [[nodiscard]] virtual bool Stop() override;

private:
    std::atomic_bool m_active;
};

//----------------------------------------------------------------------------------------------------------------------

class BackgroundRuntime final : public IRuntimePolicy
{
public:
    explicit BackgroundRuntime(BryptNode& instance);

    [[nodiscard]] virtual bool Start() override;
    [[nodiscard]] virtual bool Stop() override;

private:
    void BackgroundProcessor(std::stop_token token);

    std::atomic_bool m_active;
    std::jthread m_worker;
};

//----------------------------------------------------------------------------------------------------------------------
