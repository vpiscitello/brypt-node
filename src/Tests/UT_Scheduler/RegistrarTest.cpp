//----------------------------------------------------------------------------------------------------------------------
#include "Components/Scheduler/Registrar.hpp"
#include "Utilities/InvokeContext.hpp"
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
    explicit IndependentExecutor(std::shared_ptr<Scheduler::Registrar> const& spRegistrar);
    [[nodiscard]] bool Executed() const { return m_executed; }
    [[nodiscard]] std::size_t GetPriority() const { return m_spDelegate->GetPriority(); }
    void ResetExecutionStatus() { m_executed = false; }
private:
    std::shared_ptr<Scheduler::Delegate> m_spDelegate;
    bool m_executed;
};

//----------------------------------------------------------------------------------------------------------------------

class local::DependentExecutorAlpha
{
public:
    explicit DependentExecutorAlpha(std::shared_ptr<Scheduler::Registrar> const& spRegistrar);
    [[nodiscard]] bool Executed() const { return m_executed; }
    [[nodiscard]] std::size_t GetPriority() const { return m_spDelegate->GetPriority(); }
    void ResetExecutionStatus() { m_executed = false; }
private:
    std::shared_ptr<Scheduler::Delegate> m_spDelegate;
    bool m_executed;
};

//----------------------------------------------------------------------------------------------------------------------

class local::DependentExecutorBeta
{
public:
    explicit DependentExecutorBeta(std::shared_ptr<Scheduler::Registrar> const& spRegistrar);
    [[nodiscard]] bool Executed() const { return m_executed; }
    [[nodiscard]] std::size_t GetPriority() const { return m_spDelegate->GetPriority(); }
    void ResetExecutionStatus() { m_executed = false; }
private:
    std::shared_ptr<Scheduler::Delegate> m_spDelegate;
    bool m_executed;
};

//----------------------------------------------------------------------------------------------------------------------

class local::DependentExecutorGamma
{
public:
    explicit DependentExecutorGamma(std::shared_ptr<Scheduler::Registrar> const& spRegistrar);
    [[nodiscard]] bool Executed() const { return m_executed; }
    [[nodiscard]] std::size_t GetPriority() const { return m_spDelegate->GetPriority(); }
    void ResetExecutionStatus() { m_executed = false; }
private:
    std::shared_ptr<Scheduler::Delegate> m_spDelegate;
    bool m_executed;
};

//----------------------------------------------------------------------------------------------------------------------

class local::CyclicExecutorAlpha
{
public:
    explicit CyclicExecutorAlpha(std::shared_ptr<Scheduler::Registrar> const& spRegistrar);
    [[nodiscard]] std::size_t GetPriority() const { return m_spDelegate->GetPriority(); }
private:
    std::shared_ptr<Scheduler::Delegate> m_spDelegate;
};

//----------------------------------------------------------------------------------------------------------------------

class local::CyclicExecutorBeta
{
public:
    explicit CyclicExecutorBeta(std::shared_ptr<Scheduler::Registrar> const& spRegistrar);
    [[nodiscard]] std::size_t GetPriority() const { return m_spDelegate->GetPriority(); }
private:
    std::shared_ptr<Scheduler::Delegate> m_spDelegate;
};

//----------------------------------------------------------------------------------------------------------------------

TEST(SchedulerSchedulerSuite, PriorityOrderTest)
{
    auto const spRegistrar = std::make_shared<Scheduler::Registrar>();
    auto const spDependentExecutorAlpha = std::make_shared<local::DependentExecutorAlpha>(spRegistrar);
    auto const spDependentExecutorBeta = std::make_shared<local::DependentExecutorBeta>(spRegistrar);
    auto const spDependentExecutorGamma = std::make_shared<local::DependentExecutorGamma>(spRegistrar);
    auto const spIndependentExecutor = std::make_shared<local::IndependentExecutor>(spRegistrar);

    EXPECT_TRUE(spRegistrar->Initialize());

    EXPECT_EQ(spIndependentExecutor->GetPriority(), 1);
    EXPECT_EQ(spDependentExecutorAlpha->GetPriority(), 3);
    EXPECT_EQ(spDependentExecutorBeta->GetPriority(), 4);
    EXPECT_EQ(spDependentExecutorGamma->GetPriority(), 2);

    EXPECT_EQ(spRegistrar->AvailableTasks(), 4);

    auto const spIndependentDelegate = spRegistrar->GetDelegate<local::IndependentExecutor>();
    ASSERT_TRUE(spIndependentDelegate);
    EXPECT_EQ(spIndependentDelegate->AvailableTasks(), 1);

    EXPECT_EQ(spRegistrar->Execute(), 4);

    EXPECT_TRUE(spIndependentExecutor->Executed());
    EXPECT_TRUE(spDependentExecutorAlpha->Executed());
    EXPECT_TRUE(spDependentExecutorBeta->Executed());
    EXPECT_TRUE(spDependentExecutorGamma->Executed());

    EXPECT_EQ(spRegistrar->AvailableTasks(), 0);
    EXPECT_EQ(spIndependentDelegate->AvailableTasks(), 0);
}

//----------------------------------------------------------------------------------------------------------------------

TEST(SchedulerSchedulerSuite, CyclicDependencyTest)
{
    auto const spRegistrar = std::make_shared<Scheduler::Registrar>();
    auto const spIndependentExecutor = std::make_shared<local::IndependentExecutor>(spRegistrar);
    auto const spCyclicExecutorAlpha = std::make_shared<local::CyclicExecutorAlpha>(spRegistrar);
    auto const spCyclicExecutorBeta = std::make_shared<local::CyclicExecutorBeta>(spRegistrar);

    EXPECT_FALSE(spRegistrar->Initialize());

    EXPECT_EQ(spIndependentExecutor->GetPriority(), std::numeric_limits<std::size_t>::max());
    EXPECT_EQ(spCyclicExecutorAlpha->GetPriority(), std::numeric_limits<std::size_t>::max());
    EXPECT_EQ(spCyclicExecutorBeta->GetPriority(), std::numeric_limits<std::size_t>::max());
}

//----------------------------------------------------------------------------------------------------------------------

TEST(SchedulerSchedulerSuite, DelistTest)
{
    auto const spRegistrar = std::make_shared<Scheduler::Registrar>();
    auto const spIndependentExecutor = std::make_shared<local::IndependentExecutor>(spRegistrar);
    auto const spDependentExecutorBeta = std::make_shared<local::DependentExecutorBeta>(spRegistrar);
    auto const spDependentExecutorGamma = std::make_shared<local::DependentExecutorGamma>(spRegistrar);
    auto spDependentExecutorAlpha = std::make_shared<local::DependentExecutorAlpha>(spRegistrar);

    EXPECT_TRUE(spRegistrar->Initialize());

    EXPECT_EQ(spIndependentExecutor->GetPriority(), 1);
    EXPECT_EQ(spDependentExecutorAlpha->GetPriority(), 3);
    EXPECT_EQ(spDependentExecutorBeta->GetPriority(), 4);
    EXPECT_EQ(spDependentExecutorGamma->GetPriority(), 2);

    EXPECT_EQ(spRegistrar->AvailableTasks(), 4);

    {
        auto const spDelegate = spRegistrar->GetDelegate<local::DependentExecutorAlpha>();
        ASSERT_TRUE(spDelegate);
        spDelegate->Delist();
        EXPECT_EQ(spDelegate->GetPriority(), std::numeric_limits<std::size_t>::max());
        EXPECT_EQ(spRegistrar->AvailableTasks(), 3);
    }

    EXPECT_EQ(spRegistrar->Execute(), 3);

    EXPECT_TRUE(spIndependentExecutor->Executed());
    EXPECT_FALSE(spDependentExecutorAlpha->Executed());
    EXPECT_TRUE(spDependentExecutorBeta->Executed());
    EXPECT_TRUE(spDependentExecutorGamma->Executed());
    EXPECT_EQ(spRegistrar->AvailableTasks(), 0);

    {
        auto const spDelegate = spRegistrar->GetDelegate<local::DependentExecutorAlpha>();
        EXPECT_FALSE(spDelegate);
    }

    EXPECT_TRUE(spRegistrar->Initialize());
    EXPECT_EQ(spIndependentExecutor->GetPriority(), 1);
    EXPECT_EQ(spDependentExecutorAlpha->GetPriority(), std::numeric_limits<std::size_t>::max());
    EXPECT_EQ(spDependentExecutorBeta->GetPriority(), 3);
    EXPECT_EQ(spDependentExecutorGamma->GetPriority(), 2);
    
    spDependentExecutorAlpha = std::make_shared<local::DependentExecutorAlpha>(spRegistrar);
    EXPECT_EQ(spRegistrar->AvailableTasks(), 1);

    EXPECT_TRUE(spRegistrar->Initialize());
    EXPECT_EQ(spIndependentExecutor->GetPriority(), 1);
    EXPECT_EQ(spDependentExecutorAlpha->GetPriority(), 3);
    EXPECT_EQ(spDependentExecutorBeta->GetPriority(), 4);
    EXPECT_EQ(spDependentExecutorGamma->GetPriority(), 2);

    spIndependentExecutor->ResetExecutionStatus();
    spDependentExecutorAlpha->ResetExecutionStatus();
    spDependentExecutorBeta->ResetExecutionStatus();
    spDependentExecutorGamma->ResetExecutionStatus();
    
    EXPECT_EQ(spRegistrar->Execute(), 1);
    EXPECT_FALSE(spIndependentExecutor->Executed());
    EXPECT_TRUE(spDependentExecutorAlpha->Executed());
    EXPECT_FALSE(spDependentExecutorBeta->Executed());
    EXPECT_FALSE(spDependentExecutorGamma->Executed());
    EXPECT_EQ(spRegistrar->AvailableTasks(), 0);
}

//----------------------------------------------------------------------------------------------------------------------

local::IndependentExecutor::IndependentExecutor(std::shared_ptr<Scheduler::Registrar> const& spRegistrar)
    : m_spDelegate()
    , m_executed(false)
{
    auto const OnExecute = [this] () -> std::size_t { m_executed = true; return 1; };
    m_spDelegate = spRegistrar->Register<IndependentExecutor>(OnExecute);
    m_spDelegate->OnTaskAvailable();
}

//----------------------------------------------------------------------------------------------------------------------

local::DependentExecutorAlpha::DependentExecutorAlpha(std::shared_ptr<Scheduler::Registrar> const& spRegistrar)
    : m_spDelegate()
    , m_executed(false)
{
    auto const OnExecute = [this] () -> std::size_t { m_executed = true; return 1; };
    m_spDelegate = spRegistrar->Register<DependentExecutorAlpha>(OnExecute);
    m_spDelegate->Depends<DependentExecutorGamma>();
    m_spDelegate->OnTaskAvailable();
}

//----------------------------------------------------------------------------------------------------------------------

local::DependentExecutorBeta::DependentExecutorBeta(std::shared_ptr<Scheduler::Registrar> const& spRegistrar)
    : m_spDelegate()
    , m_executed(false)
{
    auto const OnExecute = [this] () -> std::size_t { m_executed = true; return 1; };
    m_spDelegate = spRegistrar->Register<DependentExecutorBeta>(OnExecute);
    m_spDelegate->Depends<DependentExecutorAlpha, DependentExecutorGamma>();
    m_spDelegate->OnTaskAvailable();
}

//----------------------------------------------------------------------------------------------------------------------

local::DependentExecutorGamma::DependentExecutorGamma(std::shared_ptr<Scheduler::Registrar> const& spRegistrar)
    : m_spDelegate()
    , m_executed(false)
{
    auto const OnExecute = [this] () -> std::size_t { m_executed = true; return 1; };
    m_spDelegate = spRegistrar->Register<DependentExecutorGamma>(OnExecute);
    m_spDelegate->Depends<IndependentExecutor>();
    m_spDelegate->OnTaskAvailable();
}

//----------------------------------------------------------------------------------------------------------------------

local::CyclicExecutorAlpha::CyclicExecutorAlpha(std::shared_ptr<Scheduler::Registrar> const& spRegistrar)
    : m_spDelegate()
{
    auto const OnExecute = [this] () -> std::size_t { return 1; };
    m_spDelegate = spRegistrar->Register<CyclicExecutorAlpha>(OnExecute);
    m_spDelegate->Depends<IndependentExecutor, CyclicExecutorBeta>();
    m_spDelegate->OnTaskAvailable();
}

//----------------------------------------------------------------------------------------------------------------------

local::CyclicExecutorBeta::CyclicExecutorBeta(std::shared_ptr<Scheduler::Registrar> const& spRegistrar)
    : m_spDelegate()
{
    auto const OnExecute = [this] () -> std::size_t { return 1; };
    m_spDelegate = spRegistrar->Register<CyclicExecutorBeta>(OnExecute);
    m_spDelegate->Depends<IndependentExecutor, CyclicExecutorAlpha>();
    m_spDelegate->OnTaskAvailable();
}

//----------------------------------------------------------------------------------------------------------------------
