//----------------------------------------------------------------------------------------------------------------------
// File: Service.hpp
// Description: 
//----------------------------------------------------------------------------------------------------------------------
#pragma once
//----------------------------------------------------------------------------------------------------------------------
#include "Delegate.hpp"
#include "Utilities/Assertions.hpp"
//----------------------------------------------------------------------------------------------------------------------
#include <atomic>
#include <cassert>
#include <condition_variable>
#include <concepts>
#include <cstdint>
#include <memory>
#include <mutex>
#include <vector>
//----------------------------------------------------------------------------------------------------------------------

namespace spdlog { class logger; }

//----------------------------------------------------------------------------------------------------------------------
namespace Scheduler {
//----------------------------------------------------------------------------------------------------------------------

class Service;

//----------------------------------------------------------------------------------------------------------------------
} // Scheduler namespace
//----------------------------------------------------------------------------------------------------------------------

class Scheduler::Service : public std::enable_shared_from_this<Service>
{
public:
    using Delegates = std::vector<std::shared_ptr<Delegate>>;

    Service();

    [[nodiscard]] bool Initialize();
    void Execute();

    [[nodiscard]] std::size_t AvailableTasks() const;
    void OnTaskAvailable(Delegate::Identifier identifier, std::size_t available);
    bool AwaitTask(std::chrono::milliseconds timeout);

    template<typename ServiceType> requires std::is_class_v<ServiceType>
    std::shared_ptr<Delegate> Register(OnExecute const& callback);

    template<typename ServiceType> requires std::is_class_v<ServiceType>
    std::shared_ptr<Delegate> GetDelegate() const;

private:
    class Sentinel {
    public:
        Sentinel();

        [[nodiscard]] std::size_t AvailableTasks() const;
        void OnTaskAvailable(std::size_t available);
        void OnTaskCompleted(std::size_t completed);
        bool AwaitTask(std::chrono::milliseconds timeout);

    private:
        std::atomic_size_t m_available;
        mutable std::mutex m_mutex;
        std::condition_variable m_waiter;
    };

    std::shared_ptr<Delegate> GetDelegate(Delegate::Identifier identifier) const;
    [[nodiscard]] bool ResolveDependencies();
    [[nodiscard]] bool UpdatePriorityOrder();

    std::shared_ptr<spdlog::logger> m_logger;
    Sentinel m_sentinel;
    Delegates m_delegates;
    bool m_initialized;
};

//----------------------------------------------------------------------------------------------------------------------

template<typename ServiceType> requires std::is_class_v<ServiceType>
std::shared_ptr<Scheduler::Delegate> Scheduler::Service::Register(OnExecute const& callback)
{
    assert(Assertions::Threading::IsCoreThread());
    assert(!GetDelegate<ServiceType>()); // Currently, only one delegate per service type is supported. 
    auto const spService = m_delegates.emplace_back(
        std::make_shared<Delegate>(typeid(ServiceType).hash_code(), callback, shared_from_this()));
    return spService;
}

//----------------------------------------------------------------------------------------------------------------------

template<typename ServiceType> requires std::is_class_v<ServiceType>
std::shared_ptr<Scheduler::Delegate> Scheduler::Service::GetDelegate() const
{
    return GetDelegate(typeid(ServiceType).hash_code());
}

//----------------------------------------------------------------------------------------------------------------------
