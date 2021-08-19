//----------------------------------------------------------------------------------------------------------------------
// File: Delegate.cpp
// Description: 
//----------------------------------------------------------------------------------------------------------------------
#include "Delegate.hpp"
#include "Service.hpp"
//----------------------------------------------------------------------------------------------------------------------
#include <limits>
//----------------------------------------------------------------------------------------------------------------------

Scheduler::Delegate::Delegate(Identifier const& identifier, OnExecute const& callback, Sentinel* const sentinel)
    : m_identifier(identifier)
    , m_priority(std::numeric_limits<std::size_t>::max())
    , m_available(0)
    , m_execute(callback)
    , m_dependencies()
    , m_sentinel(sentinel)
{
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

bool Scheduler::Delegate::IsReady() const { return m_available != 0; }

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

std::size_t Scheduler::Delegate::Execute([[maybe_unused]] ExecuteKey key)
{
    assert(m_execute && m_available != 0);
    std::size_t const completed = m_execute();
    m_available -= completed;
    return completed;
}

//----------------------------------------------------------------------------------------------------------------------

void Scheduler::Delegate::Depends(Dependencies&& dependencies)
{
    m_dependencies = std::move(dependencies);
}

//----------------------------------------------------------------------------------------------------------------------
