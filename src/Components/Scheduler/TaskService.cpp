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
    m_spDelegate = spRegistrar->Register<TaskService>([this] () -> std::size_t {
        auto const tasks = ( std::scoped_lock{ m_mutex }, std::exchange(m_tasks, {}) );
        std::ranges::for_each(tasks, [] (auto const& callback) { callback(); });
        return tasks.size();
    }); 
    assert(m_spDelegate);
}

//----------------------------------------------------------------------------------------------------------------------

Scheduler::TaskService::~TaskService()
{
    m_spDelegate->Delist();
}

//----------------------------------------------------------------------------------------------------------------------

void Scheduler::TaskService::Schedule(BasicTask const& callback)
{
	std::scoped_lock lock(m_mutex);
	m_tasks.emplace_back(callback);
	m_spDelegate->OnTaskAvailable();
}

//----------------------------------------------------------------------------------------------------------------------
