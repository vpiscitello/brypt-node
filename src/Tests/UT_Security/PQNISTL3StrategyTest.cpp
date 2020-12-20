//------------------------------------------------------------------------------------------------
#include "../../Components/Security/PostQuantum/NISTSecurityLevelThree.hpp"
#include "../../Interfaces/SecurityStrategy.hpp"
//------------------------------------------------------------------------------------------------
#include <gtest/gtest.h>
//------------------------------------------------------------------------------------------------
#include <memory>
//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
namespace {
namespace local {
//------------------------------------------------------------------------------------------------


//------------------------------------------------------------------------------------------------
} // local namespace
//------------------------------------------------------------------------------------------------
namespace test {
//------------------------------------------------------------------------------------------------


//------------------------------------------------------------------------------------------------
} // local namespace
} // namespace
//------------------------------------------------------------------------------------------------

TEST(PQNISTL3StrategySuite, UniqueContextTest)
{
    Security::PQNISTL3::CStrategy strategy(Security::Role::Initiator, Security::Context::Unique);
    EXPECT_EQ(strategy.GetRoleType(), Security::Role::Initiator);

    Security::Strategy type = strategy.GetStrategyType();
    EXPECT_EQ(type, Security::PQNISTL3::CStrategy::Type);
    EXPECT_EQ(type, Security::Strategy::PQNISTL3);

    EXPECT_EQ(strategy.GetContextType(), Security::Context::Unique);

    Security::PQNISTL3::CStrategy checkStrategy(Security::Role::Initiator, Security::Context::Unique);
    EXPECT_EQ(checkStrategy.GetRoleType(), Security::Role::Initiator);

    auto spContext = strategy.GetSessionContext().lock();
    auto spCheckContext = checkStrategy.GetSessionContext().lock();
    ASSERT_TRUE(spContext && spCheckContext);
    EXPECT_NE(spContext, spCheckContext);
    EXPECT_NE(spContext->GetPublicKey(), spCheckContext->GetPublicKey());
}

//------------------------------------------------------------------------------------------------

TEST(PQNISTL3StrategySuite, ApplicationContextTest)
{
    Security::PQNISTL3::CStrategy::InitializeApplicationContext();

    Security::PQNISTL3::CStrategy strategy(Security::Role::Initiator, Security::Context::Application);
    EXPECT_EQ(strategy.GetRoleType(), Security::Role::Initiator);

    Security::Strategy type = strategy.GetStrategyType();
    EXPECT_EQ(type, Security::PQNISTL3::CStrategy::Type);
    EXPECT_EQ(type, Security::Strategy::PQNISTL3);

    EXPECT_EQ(strategy.GetContextType(), Security::Context::Application);

    Security::PQNISTL3::CStrategy checkStrategy(Security::Role::Initiator, Security::Context::Application);
    EXPECT_EQ(checkStrategy.GetRoleType(), Security::Role::Initiator);

    auto spContext = strategy.GetSessionContext().lock();
    auto spCheckContext = checkStrategy.GetSessionContext().lock();
    ASSERT_TRUE(spContext && spCheckContext);
    EXPECT_EQ(spContext, spCheckContext);
    EXPECT_EQ(spContext->GetPublicKey(), spCheckContext->GetPublicKey());

    Security::PQNISTL3::CStrategy::ShutdownApplicationContext();
}

//------------------------------------------------------------------------------------------------

TEST(PQNISTL3StrategySuite, SynchronizationTest)
{
    Security::PQNISTL3::CStrategy initiator(Security::Role::Initiator, Security::Context::Unique);
    EXPECT_EQ(initiator.GetRoleType(), Security::Role::Initiator);
    EXPECT_EQ(initiator.GetContextType(), Security::Context::Unique);
    EXPECT_EQ(initiator.GetSynchronizationStages(), std::uint32_t(1));

    Security::PQNISTL3::CStrategy acceptor(Security::Role::Acceptor, Security::Context::Unique);
    EXPECT_EQ(acceptor.GetRoleType(), Security::Role::Acceptor);
    EXPECT_EQ(acceptor.GetContextType(), Security::Context::Unique);
    EXPECT_EQ(acceptor.GetSynchronizationStages(), std::uint32_t(2));

    // Strategy synchronization setup. 
    auto const [initiatorPreparationStatus, initiatorPreparationMessage ] = initiator.PrepareSynchronization();
    EXPECT_EQ(initiatorPreparationStatus, Security::SynchronizationStatus::Processing);
    EXPECT_GT(initiatorPreparationMessage.size(), std::uint32_t(0));

    auto const [acceptorPreparationStatus, acceptorPreparationMessage ] = acceptor.PrepareSynchronization();
    EXPECT_EQ(acceptorPreparationStatus, Security::SynchronizationStatus::Processing);
    EXPECT_EQ(acceptorPreparationMessage.size(), std::uint32_t(0));

    // Acceptor strategy initialization. 
    auto const [acceptorStageOneStatus, acceptorStageOneResponse] = acceptor.Synchronize(
        initiatorPreparationMessage);
    EXPECT_EQ(acceptorStageOneStatus, Security::SynchronizationStatus::Processing);
    EXPECT_GT(acceptorStageOneResponse.size(), std::uint32_t(0));

    // Initiator strategy initialization. 
    auto const [initiatorStageOneStatus, initiatorStageOneResponse] = initiator.Synchronize(
        acceptorStageOneResponse);
    EXPECT_EQ(initiatorStageOneStatus, Security::SynchronizationStatus::Ready);
    EXPECT_GT(initiatorStageOneResponse.size(), std::uint32_t(0));

    // Acceptor strategy verification 
    auto const [acceptorStageTwoStatus, acceptorStageTwoResponse] = acceptor.Synchronize(
        initiatorStageOneResponse); 
    EXPECT_EQ(acceptorStageTwoStatus, Security::SynchronizationStatus::Ready);
    EXPECT_EQ(acceptorStageTwoResponse.size(), std::uint32_t(0));
}

//------------------------------------------------------------------------------------------------
