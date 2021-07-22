//----------------------------------------------------------------------------------------------------------------------
// File: RuntimePolicy.hpp
// Description:
//----------------------------------------------------------------------------------------------------------------------
#pragma once
//----------------------------------------------------------------------------------------------------------------------
#include "ExecutionToken.hpp"
#include "RuntimeContext.hpp"
#include "Components/MessageControl/AssociatedMessage.hpp"
#include "Utilities/ExecutionStatus.hpp"
//----------------------------------------------------------------------------------------------------------------------
#include <atomic>
#include <concepts>
#include <memory>
#include <thread>
//----------------------------------------------------------------------------------------------------------------------

//----------------------------------------------------------------------------------------------------------------------
namespace Node {
//----------------------------------------------------------------------------------------------------------------------

class Core;
class IRuntimePolicy;
class ForegroundRuntime;
class BackgroundRuntime;
    
template<typename RuntimePolicy>
concept ValidRuntimePolicy = std::derived_from<RuntimePolicy, IRuntimePolicy>;

//----------------------------------------------------------------------------------------------------------------------
} // Node namespace
//----------------------------------------------------------------------------------------------------------------------

class Node::IRuntimePolicy
{
public:
    IRuntimePolicy(Core& instance, std::reference_wrapper<ExecutionToken> const& token);
    virtual ~IRuntimePolicy() = default;

    IRuntimePolicy(IRuntimePolicy const&) = delete; 
    IRuntimePolicy(IRuntimePolicy&& ) = delete; 
    IRuntimePolicy& operator=(IRuntimePolicy const&) = delete; 
    IRuntimePolicy& operator=(IRuntimePolicy&&) = delete; 

    [[nodiscard]] virtual RuntimeContext Type() const = 0;
    [[nodiscard]] virtual ExecutionStatus Start() = 0;

protected:
    [[nodiscard]] bool IsExecutionRequested() const;
    void SetExecutionStatus(ExecutionStatus status);
    void OnExecutionStarted();
    ExecutionStatus OnExecutionStopped() const;
    void ProcessEvents();

    Core& m_instance;
    std::reference_wrapper<ExecutionToken> m_token;
};

//----------------------------------------------------------------------------------------------------------------------

class Node::ForegroundRuntime final : public IRuntimePolicy
{
public:
    ForegroundRuntime(Core& instance, std::reference_wrapper<ExecutionToken> const& token);
    [[nodiscard]] virtual RuntimeContext Type() const override;
    [[nodiscard]] virtual ExecutionStatus Start() override;
};

//----------------------------------------------------------------------------------------------------------------------

class Node::BackgroundRuntime final : public IRuntimePolicy
{
public:
    BackgroundRuntime(Core& instance, std::reference_wrapper<ExecutionToken> const& token);
    [[nodiscard]] virtual RuntimeContext Type() const override;
    [[nodiscard]] virtual ExecutionStatus Start() override;

private:
    void BackgroundProcessor(std::stop_token token);
    std::jthread m_worker;
};

//----------------------------------------------------------------------------------------------------------------------
