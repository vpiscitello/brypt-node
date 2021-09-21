//----------------------------------------------------------------------------------------------------------------------
// File: Assertions.hpp
// Description:
//----------------------------------------------------------------------------------------------------------------------
#pragma once
//----------------------------------------------------------------------------------------------------------------------
#if !defined(NDEBUG)
//----------------------------------------------------------------------------------------------------------------------
#include <cassert>
#include <mutex>
#include <set>
#include <shared_mutex>
#include <thread>
//----------------------------------------------------------------------------------------------------------------------

//----------------------------------------------------------------------------------------------------------------------
namespace Assertions {
//----------------------------------------------------------------------------------------------------------------------

class Threading
{
public:
    Threading(Threading const&) = delete;
    Threading& operator=(Threading const) = delete;

    // Note: This must be called inorder to add a core thread id to the allowable set. 
    [[nodiscard]] static bool RegisterCoreThread()
    {
        Instance().RegisterCoreThread({});
        return true; // Returning a boolean allows this method to be packed into an assert() call. 
    }

    // Note: This must be called inorder to add a core thread id to the allowable set. 
    [[nodiscard]] static bool WithdrawCoreThread()
    {
        Instance().WithdrawCoreThread({});
        return true;
    }

    [[nodiscard]] static bool IsCoreThread()
    {
        return Instance().IsCoreThread({});
    }

private:
    struct AccessKey { public: AccessKey() = default; };
    
    Threading() = default;

    static Threading& Instance()
    {
        static std::unique_ptr<Threading> const upInstance(new Threading());
        return *upInstance;
    }

    void RegisterCoreThread([[maybe_unused]] AccessKey key)
    {
        std::scoped_lock lock(m_mutex);
        m_threads.emplace(std::this_thread::get_id()); // Set the core's thread id to the current thread. 
    }

    void WithdrawCoreThread([[maybe_unused]] AccessKey key)
    {
        std::scoped_lock lock(m_mutex);
        m_threads.erase(std::this_thread::get_id());
    }

    [[nodiscard]] bool IsCoreThread([[maybe_unused]] AccessKey key)
    {
        std::shared_lock lock(m_mutex);
        assert(!m_threads.empty()); // The core must set the id before any components can check. 
        return m_threads.contains(std::this_thread::get_id()); // Check to see if the current thread is the core thread. 
    }

    std::shared_mutex m_mutex;
    std::set<std::thread::id> m_threads;
};

//----------------------------------------------------------------------------------------------------------------------
} // Assertions namespace
//----------------------------------------------------------------------------------------------------------------------
#endif // !defined(NDEBUG)
//----------------------------------------------------------------------------------------------------------------------
