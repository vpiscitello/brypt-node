//----------------------------------------------------------------------------------------------------------------------
// File: Delegate.hpp
// Description: 
//----------------------------------------------------------------------------------------------------------------------
#pragma once
//----------------------------------------------------------------------------------------------------------------------
#include "Tasks.hpp"
//----------------------------------------------------------------------------------------------------------------------
#include <atomic>
#include <cstdint>
#include <functional>
#include <memory>
#include <set>
//----------------------------------------------------------------------------------------------------------------------

//----------------------------------------------------------------------------------------------------------------------
namespace Scheduler {
//----------------------------------------------------------------------------------------------------------------------

using OnExecute = std::function<std::size_t(Frame const&)>;

class Delegate;
class Sentinel;

//----------------------------------------------------------------------------------------------------------------------
} // Scheduler namespace
//----------------------------------------------------------------------------------------------------------------------

class Scheduler::Delegate
{
public:
    using Identifier = std::size_t;
    using Dependencies = std::set<Identifier>;

    // Note: Only the scheduler should be used to set the priority and  execute the service. 
    class ExecuteKey { public: friend class Registrar; private: ExecuteKey() = default; };

    Delegate(Identifier const& identifier, OnExecute const& callback, Sentinel* const sentinel);

    [[nodiscard]] Identifier GetIdentifier() const;
    [[nodiscard]] std::size_t GetPriority() const;
    [[nodiscard]] std::size_t AvailableTasks() const;
    [[nodiscard]] bool Ready() const;
    [[nodiscard]] std::set<Identifier> const& GetDependencies() const;

    void OnTaskAvailable(std::size_t available = 1);
    void SetPriority(ExecuteKey key, std::size_t priority);
    TaskIdentifier const& Schedule(IntervalTask::Callback const& callback, Interval const& interval);

    [[nodiscard]] std::size_t Execute(ExecuteKey key, Frame const& frame);

    // Note: When a services is dependent upon the latest execution state of another service it should is the 
    // Depends() method to identify those services. This will ensure the current service is executed after 
    // its dependencies. For example, a message processor needs the latest bootstrap cache state to ensure 
    // the most up to date set of addresses is used to build a message response. 
    void Depends(Dependencies&& dependencies);

    template<typename... ServiceTypes>
    void Depends() { Depends({ typeid(ServiceTypes).hash_code()... }); }

    void Delist();

private:
    using TaskContainer = std::unordered_map<TaskIdentifier, std::unique_ptr<BasicTask>, TaskIdentifierHasher>;

    Identifier const m_identifier;
    std::size_t m_priority;
    std::atomic_size_t m_available;
    OnExecute const m_execute;
    TaskContainer m_tasks;
    Dependencies m_dependencies;
    Sentinel* const m_sentinel;
};

//----------------------------------------------------------------------------------------------------------------------
