//----------------------------------------------------------------------------------------------------------------------
#include "BryptMessage/ApplicationMessage.hpp"
#include "BryptMessage/MessageContext.hpp"
#include "Components/Security/PostQuantum/NISTSecurityLevelThree.hpp"
#include "Interfaces/SecurityStrategy.hpp"
//----------------------------------------------------------------------------------------------------------------------
#include <gtest/gtest.h>
//----------------------------------------------------------------------------------------------------------------------
#include <memory>
//----------------------------------------------------------------------------------------------------------------------

//----------------------------------------------------------------------------------------------------------------------
namespace {
namespace local {
//----------------------------------------------------------------------------------------------------------------------

Message::Context GenerateMessageContext(Security::PQNISTL3::Strategy const& strategy);

//----------------------------------------------------------------------------------------------------------------------
} // local namespace
//----------------------------------------------------------------------------------------------------------------------
namespace test {
//----------------------------------------------------------------------------------------------------------------------

Node::Identifier const ClientIdentifier(Node::GenerateIdentifier());
Node::Identifier const ServerIdentifier(Node::GenerateIdentifier());

constexpr std::string_view ApplicationRoute = "/request";
constexpr std::string_view Data = "Hello World!";

constexpr Network::Endpoint::Identifier const EndpointIdentifier = 1;
constexpr Network::Protocol const EndpointProtocol = Network::Protocol::TCP;

constexpr Awaitable::TrackerKey TrackerKey = {
    0x01, 0x23, 0x45, 0x67, 0x89, 0xAB, 0xCD, 0xEF,
    0xEF, 0xCD, 0xAB, 0x89, 0x67, 0x45, 0x23, 0x01
};

//----------------------------------------------------------------------------------------------------------------------
} // local namespace
} // namespace
//----------------------------------------------------------------------------------------------------------------------

TEST(PQNISTL3StrategySuite, UniqueContextTest)
{
    Security::PQNISTL3::Strategy strategy(Security::Role::Initiator, Security::Context::Unique);
    EXPECT_EQ(strategy.GetRoleType(), Security::Role::Initiator);

    Security::Strategy type = strategy.GetStrategyType();
    EXPECT_EQ(type, Security::PQNISTL3::Strategy::Type);
    EXPECT_EQ(type, Security::Strategy::PQNISTL3);

    EXPECT_EQ(strategy.GetContextType(), Security::Context::Unique);

    Security::PQNISTL3::Strategy checkStrategy(Security::Role::Initiator, Security::Context::Unique);
    EXPECT_EQ(checkStrategy.GetRoleType(), Security::Role::Initiator);

    auto spContext = strategy.GetSessionContext().lock();
    auto spCheckContext = checkStrategy.GetSessionContext().lock();
    ASSERT_TRUE(spContext && spCheckContext);
    EXPECT_NE(spContext, spCheckContext);
    EXPECT_NE(spContext->GetPublicKey(), spCheckContext->GetPublicKey());
}

//----------------------------------------------------------------------------------------------------------------------

TEST(PQNISTL3StrategySuite, ApplicationContextTest)
{
    Security::PQNISTL3::Strategy::InitializeApplicationContext();

    Security::PQNISTL3::Strategy strategy(Security::Role::Initiator, Security::Context::Application);
    EXPECT_EQ(strategy.GetRoleType(), Security::Role::Initiator);

    Security::Strategy type = strategy.GetStrategyType();
    EXPECT_EQ(type, Security::PQNISTL3::Strategy::Type);
    EXPECT_EQ(type, Security::Strategy::PQNISTL3);

    EXPECT_EQ(strategy.GetContextType(), Security::Context::Application);

    Security::PQNISTL3::Strategy checkStrategy(Security::Role::Initiator, Security::Context::Application);
    EXPECT_EQ(checkStrategy.GetRoleType(), Security::Role::Initiator);

    auto spContext = strategy.GetSessionContext().lock();
    auto spCheckContext = checkStrategy.GetSessionContext().lock();
    ASSERT_TRUE(spContext && spCheckContext);
    EXPECT_EQ(spContext, spCheckContext);
    EXPECT_EQ(spContext->GetPublicKey(), spCheckContext->GetPublicKey());

    Security::PQNISTL3::Strategy::ShutdownApplicationContext();
}

//----------------------------------------------------------------------------------------------------------------------

TEST(PQNISTL3StrategySuite, SynchronizationTest)
{
    Security::PQNISTL3::Strategy initiator(Security::Role::Initiator, Security::Context::Unique);
    EXPECT_EQ(initiator.GetRoleType(), Security::Role::Initiator);
    EXPECT_EQ(initiator.GetContextType(), Security::Context::Unique);
    EXPECT_EQ(initiator.GetSynchronizationStages(), std::uint32_t(1));

    Security::PQNISTL3::Strategy acceptor(Security::Role::Acceptor, Security::Context::Unique);
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

    // Verify that we can generate an initiator application pack that can be decrypted and verified by the acceptor. 
    std::string initiatorApplicationPack = [&] () -> std::string {
        auto const context = local::GenerateMessageContext(initiator);
        auto const optApplicationMessage = Message::Application::Parcel::GetBuilder()
            .SetContext(context)
            .SetSource(test::ClientIdentifier)
            .SetDestination(test::ServerIdentifier)
            .SetRoute(test::ApplicationRoute)
            .SetPayload(test::Data)
            .BindExtension<Message::Application::Extension::Awaitable>(
                Message::Application::Extension::Awaitable::Request, test::TrackerKey)
            .ValidatedBuild();

        return (optApplicationMessage) ? optApplicationMessage->GetPack() : "";
    }();
    ASSERT_FALSE(initiatorApplicationPack.empty());

    {
        auto const context = local::GenerateMessageContext(acceptor);
        auto const optPackMessage = Message::Application::Parcel::GetBuilder()
            .SetContext(context)
            .FromEncodedPack(initiatorApplicationPack)
            .ValidatedBuild();
        ASSERT_TRUE(optPackMessage);

        EXPECT_EQ(optPackMessage->GetSource(), test::ClientIdentifier);
        ASSERT_TRUE(optPackMessage->GetDestination());
        EXPECT_EQ(optPackMessage->GetDestination(), test::ServerIdentifier);
        EXPECT_EQ(optPackMessage->GetRoute(), test::ApplicationRoute);

        auto const optAwaitable = optPackMessage->GetExtension<Message::Application::Extension::Awaitable>();
        ASSERT_TRUE(optAwaitable);
        EXPECT_EQ(optAwaitable->get().GetBinding(), Message::Application::Extension::Awaitable::Request);
        EXPECT_EQ(optAwaitable->get().GetTracker(), test::TrackerKey);

        auto const buffer = optPackMessage->GetPayload();
        std::string const data(buffer.begin(), buffer.end());
        EXPECT_EQ(data, test::Data);  
    }

    // Verify that we can generate an acceptor application pack that can be decrypted and verified by the initiator. 
    std::string acceptorApplicationPack = [&] () -> std::string {
        auto const context = local::GenerateMessageContext(acceptor);
        auto const optApplicationMessage = Message::Application::Parcel::GetBuilder()
            .SetContext(context)
            .SetSource(test::ClientIdentifier)
            .SetDestination(test::ServerIdentifier)
            .SetRoute(test::ApplicationRoute)
            .SetPayload(test::Data)
            .BindExtension<Message::Application::Extension::Awaitable>(
                Message::Application::Extension::Awaitable::Response, test::TrackerKey)
            .ValidatedBuild();

        return (optApplicationMessage) ? optApplicationMessage->GetPack() : "";
    }();
    ASSERT_FALSE(acceptorApplicationPack.empty());

    {
        auto const context = local::GenerateMessageContext(initiator);
        auto const optPackMessage = Message::Application::Parcel::GetBuilder()
            .SetContext(context)
            .FromEncodedPack(acceptorApplicationPack)
            .ValidatedBuild();
        ASSERT_TRUE(optPackMessage);

        EXPECT_EQ(optPackMessage->GetSource(), test::ClientIdentifier);
        ASSERT_TRUE(optPackMessage->GetDestination());
        EXPECT_EQ(optPackMessage->GetDestination(), test::ServerIdentifier);
        EXPECT_EQ(optPackMessage->GetRoute(), test::ApplicationRoute);

        auto const optAwaitable = optPackMessage->GetExtension<Message::Application::Extension::Awaitable>();
        ASSERT_TRUE(optAwaitable);
        EXPECT_EQ(optAwaitable->get().GetBinding(), Message::Application::Extension::Awaitable::Response);
        EXPECT_EQ(optAwaitable->get().GetTracker(), test::TrackerKey);

        auto const buffer = optPackMessage->GetPayload();
        std::string const data(buffer.begin(), buffer.end());
        EXPECT_EQ(data, test::Data);  
    }
}

//----------------------------------------------------------------------------------------------------------------------

Message::Context local::GenerateMessageContext(Security::PQNISTL3::Strategy const& strategy)
{
    Message::Context context(test::EndpointIdentifier, test::EndpointProtocol);

    context.BindEncryptionHandlers(
        [&strategy] (auto const& buffer, auto nonce) -> Security::Encryptor::result_type { 
            return strategy.Encrypt(buffer, nonce);
        },
        [&strategy] (auto const& buffer, auto nonce) -> Security::Decryptor::result_type {
            return strategy.Decrypt(buffer, nonce);
        });

    context.BindSignatureHandlers(
        [&strategy] (auto& buffer) -> Security::Signator::result_type { return strategy.Sign(buffer); },
        [&strategy] (auto const& buffer) -> Security::Verifier::result_type { return strategy.Verify(buffer); },
        [&strategy] () -> Security::SignatureSizeGetter::result_type { return strategy.GetSignatureSize(); });

    return context;
}

//----------------------------------------------------------------------------------------------------------------------
