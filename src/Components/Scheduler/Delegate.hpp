//----------------------------------------------------------------------------------------------------------------------
// File: Delegate.hpp
// Description: 
//----------------------------------------------------------------------------------------------------------------------
#pragma once
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

using OnExecute = std::function<std::size_t()>;

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
    class ExecuteKey { public: friend class Service; private: ExecuteKey() = default; };

    Delegate(Identifier const& identifier, OnExecute const& callback, Sentinel* const sentinel);

    [[nodiscard]] Identifier GetIdentifier() const;
    [[nodiscard]] std::size_t GetPriority() const;
    [[nodiscard]] std::size_t AvailableTasks() const;
    [[nodiscard]] bool IsReady() const;
    [[nodiscard]] std::set<Identifier> const& GetDependencies() const;

    void OnTaskAvailable(std::size_t available = 1);

    void SetPriority(ExecuteKey key, std::size_t priority);
    [[nodiscard]] std::size_t Execute(ExecuteKey key);

    // Note: When a services is dependent upon the latest execution state of another service it should is the 
    // Depends() method to identify those services. This will ensure the current service is executed after 
    // its dependencies. For example, a message processor needs the latest bootstrap cache state to ensure 
    // the most up to date set of addresses is used to build a message response. 
    void Depends(Dependencies&& dependencies);

    template<typename... ServiceTypes>
    void Depends() { Depends({ typeid(ServiceTypes).hash_code()... }); }

    void Delist();

private:
    Identifier const m_identifier;
    std::size_t m_priority;
    std::atomic_size_t m_available;
    OnExecute const m_execute;
    Dependencies m_dependencies;
    Sentinel* const m_sentinel;
};

//----------------------------------------------------------------------------------------------------------------------
