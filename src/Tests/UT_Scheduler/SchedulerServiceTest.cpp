//----------------------------------------------------------------------------------------------------------------------
#include "Components/Scheduler/Service.hpp"
//----------------------------------------------------------------------------------------------------------------------
#include <gtest/gtest.h>
//----------------------------------------------------------------------------------------------------------------------

//----------------------------------------------------------------------------------------------------------------------
namespace {
namespace local {
//----------------------------------------------------------------------------------------------------------------------

class IndependentExecutor;

class DependentExecutorAlpha;
class DependentExecutorBeta;
class DependentExecutorGamma;

class CyclicExecutorAlpha;
class CyclicExecutorBeta;

//----------------------------------------------------------------------------------------------------------------------
} // local namespace
//----------------------------------------------------------------------------------------------------------------------
namespace test {
//----------------------------------------------------------------------------------------------------------------------

//----------------------------------------------------------------------------------------------------------------------
} // local namespace
} // namespace
//----------------------------------------------------------------------------------------------------------------------

class local::IndependentExecutor
{
public:
    explicit IndependentExecutor(std::shared_ptr<Scheduler::Service> const& spScheduler);
    [[nodiscard]] bool Executed() const { return m_executed; }
    [[nodiscard]] std::size_t GetPriority() const { return m_spDelegate->GetPriority(); }
private:
    std::shared_ptr<Scheduler::Delegate> m_spDelegate;
    bool m_executed;
};

//----------------------------------------------------------------------------------------------------------------------

class local::DependentExecutorAlpha
{
public:
    explicit DependentExecutorAlpha(std::shared_ptr<Scheduler::Service> const& spScheduler);
    [[nodiscard]] bool Executed() const { return m_executed; }
    [[nodiscard]] std::size_t GetPriority() const { return m_spDelegate->GetPriority(); }
private:
    std::shared_ptr<Scheduler::Delegate> m_spDelegate;
    bool m_executed;
};

//----------------------------------------------------------------------------------------------------------------------

class local::DependentExecutorBeta
{
public:
    explicit DependentExecutorBeta(std::shared_ptr<Scheduler::Service> const& spScheduler);
    [[nodiscard]] bool Executed() const { return m_executed; }
    [[nodiscard]] std::size_t GetPriority() const { return m_spDelegate->GetPriority(); }
private:
    std::shared_ptr<Scheduler::Delegate> m_spDelegate;
    bool m_executed;
};

//----------------------------------------------------------------------------------------------------------------------

class local::DependentExecutorGamma
{
public:
    explicit DependentExecutorGamma(std::shared_ptr<Scheduler::Service> const& spScheduler);
    [[nodiscard]] bool Executed() const { return m_executed; }
    [[nodiscard]] std::size_t GetPriority() const { return m_spDelegate->GetPriority(); }
private:
    std::shared_ptr<Scheduler::Delegate> m_spDelegate;
    bool m_executed;
};

//----------------------------------------------------------------------------------------------------------------------

class local::CyclicExecutorAlpha
{
public:
    explicit CyclicExecutorAlpha(std::shared_ptr<Scheduler::Service> const& spScheduler);
    [[nodiscard]] std::size_t GetPriority() const { return m_spDelegate->GetPriority(); }
private:
    std::shared_ptr<Scheduler::Delegate> m_spDelegate;
};

//----------------------------------------------------------------------------------------------------------------------

class local::CyclicExecutorBeta
{
public:
    explicit CyclicExecutorBeta(std::shared_ptr<Scheduler::Service> const& spScheduler);
    [[nodiscard]] std::size_t GetPriority() const { return m_spDelegate->GetPriority(); }
private:
    std::shared_ptr<Scheduler::Delegate> m_spDelegate;
};

//----------------------------------------------------------------------------------------------------------------------

TEST(SchedulerSchedulerSuite, PriorityOrderTest)
{
    auto const spScheduler = std::make_shared<Scheduler::Service>();
    auto const spDependentExecutorAlpha = std::make_shared<local::DependentExecutorAlpha>(spScheduler);
    auto const spDependentExecutorBeta = std::make_shared<local::DependentExecutorBeta>(spScheduler);
    auto const spDependentExecutorGamma = std::make_shared<local::DependentExecutorGamma>(spScheduler);
    auto const spIndependentExecutor = std::make_shared<local::IndependentExecutor>(spScheduler);

    EXPECT_TRUE(spScheduler->Initialize());

    EXPECT_EQ(spIndependentExecutor->GetPriority(), 1);
    EXPECT_EQ(spDependentExecutorAlpha->GetPriority(), 3);
    EXPECT_EQ(spDependentExecutorBeta->GetPriority(), 4);
    EXPECT_EQ(spDependentExecutorGamma->GetPriority(), 2);

    EXPECT_EQ(spScheduler->AvailableTasks(), 4);

    auto const spIndependentDelegate = spScheduler->GetDelegate<local::IndependentExecutor>();
    ASSERT_TRUE(spIndependentDelegate);
    EXPECT_EQ(spIndependentDelegate->AvailableTasks(), 1);

    spScheduler->Execute();

    EXPECT_TRUE(spIndependentExecutor->Executed());
    EXPECT_TRUE(spDependentExecutorAlpha->Executed());
    EXPECT_TRUE(spDependentExecutorBeta->Executed());
    EXPECT_TRUE(spDependentExecutorGamma->Executed());

    EXPECT_EQ(spScheduler->AvailableTasks(), 0);
    EXPECT_EQ(spIndependentDelegate->AvailableTasks(), 0);
}

//----------------------------------------------------------------------------------------------------------------------

TEST(SchedulerSchedulerSuite, CyclicDependencyTest)
{
    auto const spScheduler = std::make_shared<Scheduler::Service>();
    auto const spIndependentExecutor = std::make_shared<local::IndependentExecutor>(spScheduler);
    auto const spCyclicExecutorAlpha = std::make_shared<local::CyclicExecutorAlpha>(spScheduler);
    auto const spCyclicExecutorBeta = std::make_shared<local::CyclicExecutorBeta>(spScheduler);

    EXPECT_FALSE(spScheduler->Initialize());

    EXPECT_EQ(spIndependentExecutor->GetPriority(), std::numeric_limits<std::size_t>::max());
    EXPECT_EQ(spCyclicExecutorAlpha->GetPriority(), std::numeric_limits<std::size_t>::max());
    EXPECT_EQ(spCyclicExecutorBeta->GetPriority(), std::numeric_limits<std::size_t>::max());
}

//----------------------------------------------------------------------------------------------------------------------

local::IndependentExecutor::IndependentExecutor(std::shared_ptr<Scheduler::Service> const& spScheduler)
    : m_spDelegate()
    , m_executed(false)
{
    auto const OnExecute = [this] () -> std::size_t { m_executed = true; return 1; };
    m_spDelegate = spScheduler->Register<IndependentExecutor>(OnExecute);
    m_spDelegate->OnTaskAvailable();
}

//----------------------------------------------------------------------------------------------------------------------

local::DependentExecutorAlpha::DependentExecutorAlpha(std::shared_ptr<Scheduler::Service> const& spScheduler)
    : m_spDelegate()
    , m_executed(false)
{
    auto const OnExecute = [this] () -> std::size_t { m_executed = true; return 1; };
    m_spDelegate = spScheduler->Register<DependentExecutorAlpha>(OnExecute);
    m_spDelegate->Depends<DependentExecutorGamma>();
    m_spDelegate->OnTaskAvailable();
}

//----------------------------------------------------------------------------------------------------------------------

local::DependentExecutorBeta::DependentExecutorBeta(std::shared_ptr<Scheduler::Service> const& spScheduler)
    : m_spDelegate()
    , m_executed(false)
{
    auto const OnExecute = [this] () -> std::size_t { m_executed = true; return 1; };
    m_spDelegate = spScheduler->Register<DependentExecutorBeta>(OnExecute);
    m_spDelegate->Depends<DependentExecutorAlpha>();
    m_spDelegate->OnTaskAvailable();
}

//----------------------------------------------------------------------------------------------------------------------

local::DependentExecutorGamma::DependentExecutorGamma(std::shared_ptr<Scheduler::Service> const& spScheduler)
    : m_spDelegate()
    , m_executed(false)
{
    auto const OnExecute = [this] () -> std::size_t { m_executed = true; return 1; };
    m_spDelegate = spScheduler->Register<DependentExecutorGamma>(OnExecute);
    m_spDelegate->Depends<IndependentExecutor>();
    m_spDelegate->OnTaskAvailable();
}

//----------------------------------------------------------------------------------------------------------------------

local::CyclicExecutorAlpha::CyclicExecutorAlpha(std::shared_ptr<Scheduler::Service> const& spScheduler)
    : m_spDelegate()
{
    auto const OnExecute = [this] () -> std::size_t { return 1; };
    m_spDelegate = spScheduler->Register<CyclicExecutorAlpha>(OnExecute);
    m_spDelegate->Depends<IndependentExecutor, CyclicExecutorBeta>();
    m_spDelegate->OnTaskAvailable();
}

//----------------------------------------------------------------------------------------------------------------------

local::CyclicExecutorBeta::CyclicExecutorBeta(std::shared_ptr<Scheduler::Service> const& spScheduler)
    : m_spDelegate()
{
    auto const OnExecute = [this] () -> std::size_t { return 1; };
    m_spDelegate = spScheduler->Register<CyclicExecutorBeta>(OnExecute);
    m_spDelegate->Depends<IndependentExecutor, CyclicExecutorAlpha>();
    m_spDelegate->OnTaskAvailable();
}

//----------------------------------------------------------------------------------------------------------------------
