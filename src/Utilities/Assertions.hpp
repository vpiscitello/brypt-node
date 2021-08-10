//----------------------------------------------------------------------------------------------------------------------
// File: Assertions.hpp
// Description:
//----------------------------------------------------------------------------------------------------------------------
#pragma once
//----------------------------------------------------------------------------------------------------------------------
#if !defined(NDEBUG)
//----------------------------------------------------------------------------------------------------------------------
#include <cassert>
#include <atomic>
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

    // Note: main() must call this method to ensure the main thread is set as the stored identifier. 
    [[nodiscard]] static bool SetCoreThread()
    {
        Instance().SetCoreThread({});
        return true; // Returning a boolean allows this method to be packed into an assert() call. 
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

    void SetCoreThread([[maybe_unused]] AccessKey key)
    {
        m_core = std::this_thread::get_id(); // Set the core's thread id to the current thread. 
    }

    [[nodiscard]] bool IsCoreThread([[maybe_unused]] AccessKey key)
    {
        assert(m_core != std::thread::id()); // The core must set the id before any components can check. 
        return m_core == std::this_thread::get_id(); // Check to see if the current thread is the core thread. 
    }

    std::atomic<std::thread::id> m_core;
};

//----------------------------------------------------------------------------------------------------------------------
} // Assertions namespace
//----------------------------------------------------------------------------------------------------------------------
#endif // !defined(NDEBUG)
//----------------------------------------------------------------------------------------------------------------------
