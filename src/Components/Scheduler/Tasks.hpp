//----------------------------------------------------------------------------------------------------------------------
// File: Tasks.hpp
// Description: 
//----------------------------------------------------------------------------------------------------------------------
#pragma once
//----------------------------------------------------------------------------------------------------------------------
#include "Utilities/StrongType.hpp"
//----------------------------------------------------------------------------------------------------------------------
#include <cassert>
#include <chrono>
#include <compare>
#include <functional>
//----------------------------------------------------------------------------------------------------------------------

//----------------------------------------------------------------------------------------------------------------------
namespace Scheduler {
//----------------------------------------------------------------------------------------------------------------------

using Frame = StrongType<std::uint32_t, struct FrameTag>;
using Interval = StrongType<Frame::UnderlyingType, struct IntervalTag>;

class TaskIdentifier;
struct TaskIdentifierHasher;

class BasicTask;
class OneShotTask;
class IntervalTask;

//----------------------------------------------------------------------------------------------------------------------
} // Scheduler namespace
//----------------------------------------------------------------------------------------------------------------------

[[nodiscard]] inline Scheduler::Frame::UnderlyingType operator%(
    Scheduler::Frame const& frame, Scheduler::Interval const& interval)
{
    return frame.GetValue() % interval.GetValue();
}

//----------------------------------------------------------------------------------------------------------------------

class Scheduler::TaskIdentifier
{
public:
    using UnderlyingType = std::uint32_t;

    TaskIdentifier() : m_value(Generator::Instance().Generate()) {}
    TaskIdentifier(TaskIdentifier const& other) : m_value(other.m_value) {}
    TaskIdentifier(TaskIdentifier&& other) noexcept : m_value(std::move(other.m_value)) {}
    TaskIdentifier(UnderlyingType const& value) : m_value(value) {}
    TaskIdentifier(UnderlyingType&& value) : m_value(std::move(value)) {}

    [[nodiscard]] bool operator==(TaskIdentifier const& other) const
    {
        return m_value == other.m_value;
    }

    [[nodiscard]] std::strong_ordering operator<=>(TaskIdentifier const& other) const
    {
        return m_value <=> other.m_value;
    }

    UnderlyingType& GetValue() { return m_value; }
    UnderlyingType const& GetValue() const { return m_value; }

private:
    class Generator
    {
    public:
        static Generator& Instance() {
            static Generator instance;
            return instance;
        }

        Generator(Generator const&) = delete;
        void operator=(Generator const&) = delete;

        UnderlyingType Generate() { return ++m_counter; }
        
    private:
        constexpr Generator() : m_counter(0) { }

        TaskIdentifier::UnderlyingType m_counter;
    };

    UnderlyingType m_value;
};

//----------------------------------------------------------------------------------------------------------------------

struct Scheduler::TaskIdentifierHasher 
{
    std::size_t operator() (TaskIdentifier const& identifier) const
    {
        return std::hash<TaskIdentifier::UnderlyingType>()(identifier.GetValue());
    }
};

//----------------------------------------------------------------------------------------------------------------------

class Scheduler::BasicTask
{
public:
    using Callback = std::function<void()>;

    BasicTask(Callback const& callback, bool repeat)
        : m_callback(callback)
        , m_repeat(repeat)
    {
        assert(callback);
    }

    virtual ~BasicTask() = default;

    [[nodiscard]] virtual bool Ready(Frame const&) { return true; }
    [[nodiscard]] bool Repeat() const { return m_repeat; }
    void Execute() const { m_callback(); }

private:
    Callback m_callback;
    bool m_repeat;
};

//----------------------------------------------------------------------------------------------------------------------

class Scheduler::OneShotTask : public Scheduler::BasicTask
{
public:
    explicit OneShotTask(Callback const& callback) : BasicTask(callback, false) { }
};

//----------------------------------------------------------------------------------------------------------------------

class Scheduler::IntervalTask : public Scheduler::BasicTask
{
public:
    IntervalTask(Callback const& callback, Interval const& interval)
        : BasicTask(callback, true)
        , m_interval(interval)
        , m_updated(0)
    {
        assert(m_interval != Interval{ 0 });
    }

    // BasicTask {
    [[nodiscard]] virtual bool Ready(Frame const& frame) override
    {
        if (frame % m_interval != 0) { return false; }
        m_updated = frame;
        return true;
    }
    // } BasicTask

private:
    Interval m_interval;
    Frame m_updated;
};

//----------------------------------------------------------------------------------------------------------------------
