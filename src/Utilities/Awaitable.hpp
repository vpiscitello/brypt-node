//----------------------------------------------------------------------------------------------------------------------
// File: Awaitable.hpp
// Description:
//----------------------------------------------------------------------------------------------------------------------
#pragma once
//----------------------------------------------------------------------------------------------------------------------
#include <atomic>
#include <cassert>
#include <compare>
#include <coroutine>
#include <memory>
//----------------------------------------------------------------------------------------------------------------------

//----------------------------------------------------------------------------------------------------------------------
namespace Awaitable {
//----------------------------------------------------------------------------------------------------------------------

enum class Result : std::uint32_t { Signaled, Canceled };

class ExclusiveSignal;

//----------------------------------------------------------------------------------------------------------------------
} // Awaitable namespace
//----------------------------------------------------------------------------------------------------------------------

class Awaitable::ExclusiveSignal
{
public:
    ExclusiveSignal() : m_spState(std::make_shared<State>()) { }
    ~ExclusiveSignal() = default;
    
    ExclusiveSignal(ExclusiveSignal const& other) = delete;
    ExclusiveSignal(ExclusiveSignal&& other) = default;
    
    ExclusiveSignal& operator=(ExclusiveSignal const& other) = delete;
    ExclusiveSignal& operator=(ExclusiveSignal&& other) = default;

    // Note: This method mimics the effects of the r-value copy operator with an l-value reference.
    ExclusiveSignal& ReferenceMove(ExclusiveSignal& other)
    { 
        m_spState = std::move(other.m_spState);
        other.m_spState = nullptr;
        return *this;
    }

    [[nodiscard]] bool Ready() const noexcept { assert(m_spState); return m_spState->Ready(); }
    [[nodiscard]] bool Signaled() const noexcept { assert(m_spState); return m_spState->Signaled(); }
    [[nodiscard]] bool Canceled() const noexcept { assert(m_spState); return m_spState->Canceled(); }
    [[nodiscard]] bool Waiting() const noexcept { assert(m_spState); return m_spState->Waiting(); }

    void Notify() noexcept { assert(m_spState && !m_spState->Signaled()); m_spState->Notify(); }
    void Cancel() const noexcept { assert(m_spState); return m_spState->Cancel(); }
    [[nodiscard]] auto AsyncWait() const noexcept { assert(m_spState); return Awaiter{*m_spState}; }

private:
    class Phase
    {
    public:
        enum Value : std::uint32_t { Ready, Signal, Cancel };
        Phase() : m_value(Ready) {}
        operator Value() const noexcept { return m_value.load(std::memory_order::relaxed); }
        Phase& operator=(Value value) noexcept { m_value.exchange(value, std::memory_order::acq_rel); return *this; }
        auto operator<=>(Value value) const noexcept { return (m_value.load(std::memory_order::acquire) <=> value); }

    private:
        std::atomic<Value> m_value;
    };

    class State
    {
    public:
        State() = default;
        ~State() { Cancel(); }

        [[nodiscard]] bool Ready() const noexcept { return m_phase == Phase::Ready; }
        [[nodiscard]] bool Signaled() const noexcept { return m_phase == Phase::Signal; }
        [[nodiscard]] bool Canceled() const noexcept { return m_phase == Phase::Cancel; }
        [[nodiscard]] bool Waiting() const noexcept { return static_cast<bool>(m_handle); }

        void OnWait(std::coroutine_handle<> handle) noexcept { assert(!m_handle); m_handle = handle; }

        void Notify() noexcept  { Resume(Phase::Signal); }
        void Cancel() noexcept { Resume(Phase::Cancel); }
        void Reset() noexcept { m_phase = Phase::Ready; m_handle = nullptr; }

    private:
        void Resume(Phase::Value value) noexcept { m_phase = value; if (m_handle) { m_handle.resume(); } }

        Phase m_phase;
        std::coroutine_handle<> m_handle;
    };

    class Awaiter
    {
    public:
        explicit Awaiter(State& state) : m_state(state) {}

        // Note: The calling coroutine will only be suspended if the signal has not been set. 
        [[nodiscard]] bool await_ready() const noexcept { return !m_state.Ready(); }

        // Note: The coroutine will always be suspended if ready. 
        void await_suspend(std::coroutine_handle<> handle) noexcept { m_state.OnWait(handle); }

        // Note: The signal will be automatically reset when the waiting coroutine is resumed.  
        [[nodiscard]] Result await_resume() const noexcept
        {
            assert(!m_state.Ready()); // The coroutine should never be resumed in the ready phase. 
            bool const signaled = m_state.Signaled(); // Determine if we are resuming on a signal or cancellation. 
            m_state.Reset(); // Reset the phase and handle to the ready phase. 
            return (signaled) ? Result::Signaled : Result::Canceled; // Provide the caller the wait result. 
        }

    private:
        State& m_state;
    };
    
    std::shared_ptr<State> m_spState; // Note: Using std::shared_ptr enables copys and waiting in const methods.
};

//----------------------------------------------------------------------------------------------------------------------
