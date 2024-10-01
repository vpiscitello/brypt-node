//----------------------------------------------------------------------------------------------------------------------
// File: TaskService.hpp
// Description: 
//----------------------------------------------------------------------------------------------------------------------
#pragma once
//----------------------------------------------------------------------------------------------------------------------
#include "Tasks.hpp"
//----------------------------------------------------------------------------------------------------------------------
#include <chrono>
#include <deque>
#include <functional>
#include <memory>
#include <mutex>
//----------------------------------------------------------------------------------------------------------------------

//----------------------------------------------------------------------------------------------------------------------
namespace Scheduler {
//----------------------------------------------------------------------------------------------------------------------

class Delegate;
class Registrar;
class TaskService;

//----------------------------------------------------------------------------------------------------------------------
} // Scheduler namespace
//----------------------------------------------------------------------------------------------------------------------

class Scheduler::TaskService
{
public:
    explicit TaskService(std::shared_ptr<Registrar> const& spRegistrar);
    ~TaskService();

    void Schedule(OneShotTask::Callback const& callback);

    [[nodiscard]] std::size_t Execute(Scheduler::Frame const& frame);

private:
    using Tasks = std::deque<std::unique_ptr<BasicTask>>;
    std::shared_ptr<Delegate> m_spDelegate;
    mutable std::mutex m_mutex;
    Tasks m_tasks;
};

//----------------------------------------------------------------------------------------------------------------------
