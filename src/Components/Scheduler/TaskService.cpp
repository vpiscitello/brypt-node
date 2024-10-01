//----------------------------------------------------------------------------------------------------------------------
// File: TaskService.cpp
// Description: 
//----------------------------------------------------------------------------------------------------------------------
#include "TaskService.hpp"
#include "Registrar.hpp"
#include "Utilities/Assertions.hpp"
//----------------------------------------------------------------------------------------------------------------------
#include <algorithm>
#include <ranges>
//----------------------------------------------------------------------------------------------------------------------

Scheduler::TaskService::TaskService(std::shared_ptr<Registrar> const& spRegistrar)
    : m_spDelegate()
    , m_mutex()
    , m_tasks()
{
    assert(Assertions::Threading::IsCoreThread());
    m_spDelegate = spRegistrar->Register<TaskService>([this] (Scheduler::Frame const& frame) -> std::size_t {
        return Execute(frame);
    }); 
    assert(m_spDelegate);
}

//----------------------------------------------------------------------------------------------------------------------

Scheduler::TaskService::~TaskService()
{
    m_spDelegate->Delist();
}

//----------------------------------------------------------------------------------------------------------------------

void Scheduler::TaskService::Schedule(OneShotTask::Callback const& callback)
{
	std::scoped_lock lock(m_mutex);
	m_tasks.emplace_back(std::make_unique<OneShotTask>(callback));
	m_spDelegate->OnTaskAvailable();
}

//----------------------------------------------------------------------------------------------------------------------

std::size_t Scheduler::TaskService::Execute(Scheduler::Frame const& frame)
{
    assert(Assertions::Threading::IsCoreThread());

    auto tasks = ( std::scoped_lock{ m_mutex }, std::exchange(m_tasks, {}) );
    Tasks rescheduled;
    std::ranges::for_each(tasks, [&frame, &rescheduled] (auto&& upTask) { 
        bool const ready = upTask->Ready(frame);
        if (ready) { upTask->Execute(); }
        if (!ready || upTask->Repeat()) { rescheduled.emplace_back(std::move(upTask)); }
    });
    
    std::size_t const executed = tasks.size() - rescheduled.size();
    {
        std::scoped_lock lock{ m_mutex };
        std::ranges::for_each(rescheduled, [this] (auto&& upTask) { m_tasks.emplace_back(std::move(upTask)); });
    }
    
    return executed;
}

//----------------------------------------------------------------------------------------------------------------------
