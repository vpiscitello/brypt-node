//----------------------------------------------------------------------------------------------------------------------
// File: Delegate.cpp
// Description: 
//----------------------------------------------------------------------------------------------------------------------
#include "Delegate.hpp"
#include "Registrar.hpp"
#include "Utilities/Assertions.hpp"
//----------------------------------------------------------------------------------------------------------------------
#include <limits>
//----------------------------------------------------------------------------------------------------------------------

Scheduler::Delegate::Delegate(Identifier const& identifier, OnExecute const& callback, Sentinel* const sentinel)
    : m_identifier(identifier)
    , m_priority(std::numeric_limits<std::size_t>::max())
    , m_available(0)
    , m_execute(callback)
    , m_tasks()
    , m_dependencies()
    , m_sentinel(sentinel)
{
    assert(Assertions::Threading::IsCoreThread());
    assert(m_sentinel);
}

//----------------------------------------------------------------------------------------------------------------------

Scheduler::Delegate::Identifier Scheduler::Delegate::GetIdentifier() const { return m_identifier; }

//----------------------------------------------------------------------------------------------------------------------

std::set<Scheduler::Delegate::Identifier> const& Scheduler::Delegate::GetDependencies() const { return m_dependencies; }

//----------------------------------------------------------------------------------------------------------------------

std::size_t Scheduler::Delegate::GetPriority() const { return m_priority; }

//----------------------------------------------------------------------------------------------------------------------

std::size_t Scheduler::Delegate::AvailableTasks() const { return m_available; }

//----------------------------------------------------------------------------------------------------------------------

bool Scheduler::Delegate::Ready() const { return m_available != 0 || !m_tasks.empty(); }

//----------------------------------------------------------------------------------------------------------------------

void Scheduler::Delegate::OnTaskAvailable(std::size_t available)
{
    m_available += available;
    m_sentinel->OnTaskAvailable(available);
}

//----------------------------------------------------------------------------------------------------------------------

void Scheduler::Delegate::SetPriority([[maybe_unused]] ExecuteKey key, std::size_t priority)
{
    m_priority = priority;
}

//----------------------------------------------------------------------------------------------------------------------

Scheduler::TaskIdentifier const& Scheduler::Delegate::Schedule(
    IntervalTask::Callback const& callback, Interval const& interval)
{
    assert(Assertions::Threading::IsCoreThread());
    auto const [itr, emplaced] = m_tasks.emplace(TaskIdentifier{}, std::make_unique<IntervalTask>(callback, interval));
    assert(emplaced);
    return itr->first;
}

//----------------------------------------------------------------------------------------------------------------------

std::size_t Scheduler::Delegate::Execute([[maybe_unused]] ExecuteKey key, Frame const& frame)
{
    assert(Assertions::Threading::IsCoreThread());

    // First run the scheduled tasks. These are tasks that do not represent a single unit of work (i.e. a state checker).
    for (auto const& [identifier, upTask] : m_tasks) {
        assert(upTask);
        if (upTask->Ready(frame)) { upTask->Execute(); }
    }
    
    // Run the main work executor registered with the delegate. 
    std::size_t completed = 0;
    if (m_available != 0) {
        assert(m_execute);
        completed = m_execute(frame);
        m_available -= completed; // Decrement the amount of work available by the number completed. 
    }

    return completed; // Provide the registrar the units of work completed. 
}

//----------------------------------------------------------------------------------------------------------------------

void Scheduler::Delegate::Depends(Dependencies&& dependencies)
{
    m_dependencies = std::move(dependencies);
}

//----------------------------------------------------------------------------------------------------------------------

void Scheduler::Delegate::Delist()
{
    assert(m_sentinel);
    m_sentinel->Delist(m_identifier);
    m_priority = std::numeric_limits<std::size_t>::max();
}

//----------------------------------------------------------------------------------------------------------------------
