//----------------------------------------------------------------------------------------------------------------------
// File: Awaitable.hpp
// Description:
//----------------------------------------------------------------------------------------------------------------------
#pragma once
//----------------------------------------------------------------------------------------------------------------------
#include <atomic>
#include <cassert>
#include <coroutine>
#include <memory>
//----------------------------------------------------------------------------------------------------------------------

//----------------------------------------------------------------------------------------------------------------------
namespace Awaitable {
//----------------------------------------------------------------------------------------------------------------------

class ExclusiveSignal;

//----------------------------------------------------------------------------------------------------------------------
} // Awaitable namespace
//----------------------------------------------------------------------------------------------------------------------

class Awaitable::ExclusiveSignal
{
public:
    ExclusiveSignal() : m_upState(std::make_unique<State>()) { }
    ~ExclusiveSignal() = default;
    
    ExclusiveSignal(ExclusiveSignal const& other) = delete;
    ExclusiveSignal(ExclusiveSignal&& other) = delete;
    ExclusiveSignal& operator=(ExclusiveSignal const& other) = delete;
    ExclusiveSignal& operator=(ExclusiveSignal&& other) = delete;

    void Notify() noexcept { assert(m_upState && !m_upState->Signaled()); m_upState->Notify(); }
    [[nodiscard]] auto Wait() const noexcept { assert(m_upState); return Awaiter{*m_upState}; }

private:
    class Flag
    {
    public:
        Flag() = default;
        void Signal() noexcept { m_value.test_and_set(std::memory_order::acq_rel); }
        void Reset() noexcept { m_value.clear(std::memory_order::relaxed); }
        [[nodiscard]] bool Signaled() const noexcept { return m_value.test(std::memory_order::acquire); }
    private:
        std::atomic_flag m_value;
    };

    class State
    {
    public:
        State() = default;
        void Notify() noexcept { m_flag.Signal(); m_handle.resume(); }
        void Reset() noexcept { m_flag.Reset(); m_handle = {}; }
        void SetHandle(std::coroutine_handle<> handle) noexcept { assert(!m_handle); m_handle = handle; }
        [[nodiscard]] bool HasHandle() const noexcept { return bool(m_handle) == true; }
        [[nodiscard]] bool Signaled() const noexcept { return m_flag.Signaled(); }
    private:
        Flag m_flag;
        std::coroutine_handle<> m_handle;
    };

    class Awaiter
    {
    public:
        explicit Awaiter(State& state) : m_state(state) {}
        // Note: The calling coroutine will only be suspended if the signal has not been set. 
        [[nodiscard]] bool await_ready() const noexcept { return m_state.Signaled(); }
        // Note: The coroutine will always be suspended if ready. 
        void await_suspend(std::coroutine_handle<> handle) noexcept { m_state.SetHandle(handle); }
        // Note: The signal will be automatically reset when the waiting coroutine is resumed.  
        void await_resume() const noexcept { m_state.Reset(); }

    private:
        State& m_state;
    };
    
    std::unique_ptr<State> const m_upState; // Note: Using std::unique_ptr enabled waiting in const methods.
};

//----------------------------------------------------------------------------------------------------------------------
