//------------------------------------------------------------------------------------------------
#include "../../BryptIdentifier/BryptIdentifier.hpp"
#include "../../BryptMessage/ApplicationMessage.hpp"
#include "../../BryptMessage/HandshakeMessage.hpp"
#include "../../BryptMessage/MessageContext.hpp"
#include "../../BryptMessage/MessageDefinitions.hpp"
#include "../../Components/BryptPeer/BryptPeer.hpp"
#include "../../Components/Endpoints/EndpointIdentifier.hpp"
#include "../../Components/Endpoints/TechnologyType.hpp"
#include "../../Components/Security/SecurityMediator.hpp"
#include "../../Interfaces/ExchangeObserver.hpp"
#include "../../Interfaces/MessageSink.hpp"
#include "../../Interfaces/SecurityStrategy.hpp"
//------------------------------------------------------------------------------------------------
#include "../../Libraries/googletest/include/gtest/gtest.h"
//------------------------------------------------------------------------------------------------
#include <memory>
//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
namespace {
namespace local {
//------------------------------------------------------------------------------------------------

class CStrategyStub;
class CMessageCollector;

//------------------------------------------------------------------------------------------------
} // local namespace
//------------------------------------------------------------------------------------------------
namespace test {
//------------------------------------------------------------------------------------------------

auto const ServerIdentifier = std::make_shared<BryptIdentifier::CContainer>(
    BryptIdentifier::Generate());
auto const ClientIdentifier = std::make_shared<BryptIdentifier::CContainer>(
    BryptIdentifier::Generate());
    
constexpr std::string_view const Message = "Hello World!";

constexpr Endpoints::EndpointIdType const EndpointIdentifier = 1;
constexpr Endpoints::TechnologyType const EndpointTechnology = Endpoints::TechnologyType::TCP;
CMessageContext const MessageContext(EndpointIdentifier, EndpointTechnology);

//------------------------------------------------------------------------------------------------
} // local namespace
} // namespace
//------------------------------------------------------------------------------------------------

class local::CStrategyStub : public ISecurityStrategy
{
public:
    CStrategyStub();

    virtual Security::Strategy GetStrategyType() const override;
    virtual Security::Role GetRoleType() const override;
    virtual Security::Context GetContextType() const override;

    virtual std::uint32_t GetSynchronizationStages() const override;
    virtual Security::SynchronizationStatus GetSynchronizationStatus() const override;
    virtual Security::SynchronizationResult PrepareSynchronization() override;
    virtual Security::SynchronizationResult Synchronize(Security::Buffer const&) override;

    virtual Security::OptionalBuffer Encrypt(
        Security::Buffer const&, std::uint32_t, std::uint64_t) const override;
    virtual Security::OptionalBuffer Decrypt(
        Security::Buffer const&, std::uint32_t, std::uint64_t) const override;

    virtual std::uint32_t Sign(Security::Buffer&) const override;
    virtual Security::VerificationStatus Verify(Security::Buffer const&) const override;

private: 
    virtual std::uint32_t Sign(
        Security::Buffer const&, Security::Buffer&) const override;

    virtual Security::OptionalBuffer GenerateSignature(
        std::uint8_t const*,
        std::uint32_t,
        std::uint8_t const*,
        std::uint32_t) const override;
};

//------------------------------------------------------------------------------------------------

class local::CMessageCollector : public IMessageSink
{
public:
    CMessageCollector();

    std::string GetCollectedPack() const;

    // IMessageSink {
    virtual bool CollectMessage(
        std::weak_ptr<CBryptPeer> const& wpBryptPeer,
        CMessageContext const& context,
        std::string_view buffer) override;
        
    virtual bool CollectMessage(
        std::weak_ptr<CBryptPeer> const& wpBryptPeer,
        CMessageContext const& context,
        Message::Buffer const& buffer) override;
    // }IMessageSink
    
private:
    std::string m_pack;

};

//------------------------------------------------------------------------------------------------

TEST(SecurityMediatorSuite, ExchangeProcessorLifecycleTest)
{
    auto upStrategyStub = std::make_unique<local::CStrategyStub>();
    auto upSecurityMediator = std::make_unique<CSecurityMediator>(
        test::ServerIdentifier, std::move(upStrategyStub), std::weak_ptr<IMessageSink>());

    auto const spBryptPeer = std::make_shared<CBryptPeer>(*test::ClientIdentifier);
    upSecurityMediator->Bind(spBryptPeer);

    auto const optMessage = CHandshakeMessage::Builder()
        .SetSource(*test::ServerIdentifier)
        .SetData(test::Message)
        .ValidatedBuild();
    ASSERT_TRUE(optMessage);

    std::string const pack = optMessage->GetPack();

    EXPECT_TRUE(spBryptPeer->ScheduleReceive({}, pack));

    // Verify the node can't forward a message through the receiver, because it has been 
    // unset by the CSecurityMediator. 
    upSecurityMediator.reset();
    EXPECT_FALSE(spBryptPeer->ScheduleReceive({}, pack));
}

//------------------------------------------------------------------------------------------------

TEST(SecurityMediatorSuite, SuccessfulExchangeTest)
{
    auto upStrategyStub = std::make_unique<local::CStrategyStub>();
    auto spCollector = std::make_shared<local::CMessageCollector>();
    auto upSecurityMediator = std::make_unique<CSecurityMediator>(
        test::ServerIdentifier, std::move(upStrategyStub), spCollector);

    auto const spBryptPeer = std::make_shared<CBryptPeer>(*test::ClientIdentifier);
    upSecurityMediator->Bind(spBryptPeer);

    auto const optMessage = CHandshakeMessage::Builder()
        .SetSource(*test::ServerIdentifier)
        .SetData(test::Message)
        .ValidatedBuild();
    ASSERT_TRUE(optMessage);

    std::string const pack = optMessage->GetPack();

    EXPECT_TRUE(spBryptPeer->ScheduleReceive({}, pack));

    // Verify the peer's receiver has been swapped to the stub message sink when the 
    // mediator is notified of a sucessful exchange. 
    upSecurityMediator->HandleExchangeClose(ExchangeStatus::Success);
    EXPECT_EQ(upSecurityMediator->GetSecurityState(), Security::State::Authorized);

    EXPECT_TRUE(spBryptPeer->ScheduleReceive({}, pack));

    // Verify the stub message sink received the message
    EXPECT_EQ(spCollector->GetCollectedPack(), pack);
}

//------------------------------------------------------------------------------------------------

TEST(SecurityMediatorSuite, FailedExchangeTest)
{
    auto upStrategyStub = std::make_unique<local::CStrategyStub>();
    auto spCollector = std::make_shared<local::CMessageCollector>();
    auto upSecurityMediator = std::make_unique<CSecurityMediator>(
        test::ServerIdentifier, std::move(upStrategyStub), spCollector);

    auto const spBryptPeer = std::make_shared<CBryptPeer>(*test::ClientIdentifier);
    upSecurityMediator->Bind(spBryptPeer);

    auto const optMessage = CHandshakeMessage::Builder()
        .SetSource(*test::ServerIdentifier)
        .SetData(test::Message)
        .ValidatedBuild();
    ASSERT_TRUE(optMessage);

    std::string const pack = optMessage->GetPack();

    EXPECT_TRUE(spBryptPeer->ScheduleReceive({}, pack));

    // Verify the peer receiver has been dropped when the tracker has been notified of a failed
    // exchange. 
    upSecurityMediator->HandleExchangeClose(ExchangeStatus::Failed);
    EXPECT_EQ(upSecurityMediator->GetSecurityState(), Security::State::Unauthorized);

    EXPECT_FALSE(spBryptPeer->ScheduleReceive({}, pack));
}

//------------------------------------------------------------------------------------------------

TEST(SecurityMediatorSuite, PQNISTL3SuccessfulExchangeTest)
{
    // Declare the server resoucres for the test. 
    std::shared_ptr<CBryptPeer> spServerPeer;
    std::unique_ptr<CSecurityMediator> upServerMediator;

    // Declare the client resources for the test. 
    std::shared_ptr<CBryptPeer> spClientPeer;
    std::unique_ptr<CSecurityMediator> upClientMediator;

    auto spCollector = std::make_shared<local::CMessageCollector>();

    // Setup the client peer and its mediator
    {
        spClientPeer = std::make_shared<CBryptPeer>(*test::ClientIdentifier);
        spClientPeer->RegisterEndpoint(
            test::EndpointIdentifier,
            test::EndpointTechnology,
            [&spServerPeer] (
                [[maybe_unused]] auto const& destination, std::string_view message) -> bool
            {
                EXPECT_TRUE(spServerPeer->ScheduleReceive(test::MessageContext, message));
                return true;
            });

        upClientMediator = std::make_unique<CSecurityMediator>(
            test::ServerIdentifier, Security::Context::Unique, spCollector);
    }

    // Setup an exchange through the mediator as the initiator.
    auto const optRequest = upClientMediator->SetupExchangeInitiator(Security::Strategy::PQNISTL3);
    ASSERT_TRUE(optRequest);
    upClientMediator->Bind(spClientPeer); // Bind the client mediator to the client peer. 

    // Setup the server peer and its mediator
    {
        spServerPeer = std::make_shared<CBryptPeer>(*test::ServerIdentifier);
        spServerPeer->RegisterEndpoint(
            test::EndpointIdentifier,
            test::EndpointTechnology,
            [&spClientPeer] (
                [[maybe_unused]] auto const& destination, std::string_view message) -> bool
            {
                EXPECT_TRUE(spClientPeer->ScheduleReceive(test::MessageContext, message));
                return true;
            });

        upServerMediator = std::make_unique<CSecurityMediator>(
            test::ClientIdentifier, Security::Context::Unique, spCollector);
    }

    // Setup an exchange through the mediator as the acceptor. 
    EXPECT_TRUE(upServerMediator->SetupExchangeAcceptor(Security::Strategy::PQNISTL3));
    upServerMediator->Bind(spServerPeer); // Bind the server mediator to the server peer. 

    EXPECT_TRUE(spServerPeer->ScheduleReceive(test::MessageContext, *optRequest));

    EXPECT_EQ(upClientMediator->GetSecurityState(), Security::State::Authorized);
    EXPECT_EQ(upServerMediator->GetSecurityState(), Security::State::Authorized);

    auto const optApplicationRequest = CApplicationMessage::Builder()
        .SetSource(*test::ClientIdentifier)
        .SetDestination(*test::ServerIdentifier)
        .SetCommand(Command::Type::Connect, 0)
        .SetData(test::Message)
        .ValidatedBuild();
    ASSERT_TRUE(optApplicationRequest);

    std::string const request = optApplicationRequest->GetPack();
    EXPECT_TRUE(spClientPeer->ScheduleReceive(test::MessageContext, request));
    EXPECT_EQ(spCollector->GetCollectedPack(), request);

    auto const optApplicationResponse = CApplicationMessage::Builder()
        .SetSource(*test::ClientIdentifier)
        .SetDestination(*test::ServerIdentifier)
        .SetCommand(Command::Type::Connect, 1)
        .SetData(test::Message)
        .ValidatedBuild();
    ASSERT_TRUE(optApplicationResponse);

    std::string const response = optApplicationResponse->GetPack();
    EXPECT_TRUE(spServerPeer->ScheduleReceive(test::MessageContext, response));
    EXPECT_EQ(spCollector->GetCollectedPack(), response);
}

//------------------------------------------------------------------------------------------------

local::CStrategyStub::CStrategyStub()
{
}

//------------------------------------------------------------------------------------------------

Security::Strategy local::CStrategyStub::GetStrategyType() const
{
    return Security::Strategy::Invalid;
}

//------------------------------------------------------------------------------------------------

Security::Role local::CStrategyStub::GetRoleType() const
{
    return Security::Role::Initiator;
}

//------------------------------------------------------------------------------------------------

Security::Context local::CStrategyStub::GetContextType() const
{
    return Security::Context::Unique;
}

//------------------------------------------------------------------------------------------------

std::uint32_t local::CStrategyStub::GetSynchronizationStages() const
{
    return 0;
}

//------------------------------------------------------------------------------------------------

Security::SynchronizationStatus local::CStrategyStub::GetSynchronizationStatus() const
{
    return Security::SynchronizationStatus::Processing;
}

//------------------------------------------------------------------------------------------------

Security::SynchronizationResult local::CStrategyStub::PrepareSynchronization()
{
    return { Security::SynchronizationStatus::Processing, {} };
}

//------------------------------------------------------------------------------------------------

Security::SynchronizationResult local::CStrategyStub::Synchronize(Security::Buffer const&)
{
    return { Security::SynchronizationStatus::Processing, {} };
}

//------------------------------------------------------------------------------------------------

Security::OptionalBuffer local::CStrategyStub::Encrypt(
    [[maybe_unused]] Security::Buffer const&,
    [[maybe_unused]] std::uint32_t,
    [[maybe_unused]] std::uint64_t) const
{
    return {};
}

//------------------------------------------------------------------------------------------------

Security::OptionalBuffer local::CStrategyStub::Decrypt(
    [[maybe_unused]] Security::Buffer const&,
    [[maybe_unused]] std::uint32_t,
    [[maybe_unused]] std::uint64_t) const
{
    return {};
}


//------------------------------------------------------------------------------------------------

std::uint32_t local::CStrategyStub::Sign(
    [[maybe_unused]] Security::Buffer&) const
{
    return 0;
}

//------------------------------------------------------------------------------------------------

Security::VerificationStatus local::CStrategyStub::Verify(
    [[maybe_unused]] Security::Buffer const&) const
{
    return Security::VerificationStatus::Failed;
}

//------------------------------------------------------------------------------------------------

std::uint32_t local::CStrategyStub::Sign(
    [[maybe_unused]] Security::Buffer const&, [[maybe_unused]] Security::Buffer&) const
{
    return 0;
}

//------------------------------------------------------------------------------------------------

Security::OptionalBuffer local::CStrategyStub::GenerateSignature(
    [[maybe_unused]] std::uint8_t const*,
    [[maybe_unused]] std::uint32_t,
    [[maybe_unused]] std::uint8_t const*,
    [[maybe_unused]] std::uint32_t) const
{
    return {};
}

//------------------------------------------------------------------------------------------------

local::CMessageCollector::CMessageCollector()
    : m_pack()
{
}

//------------------------------------------------------------------------------------------------

std::string local::CMessageCollector::GetCollectedPack() const
{
    return m_pack;
}

//------------------------------------------------------------------------------------------------

bool local::CMessageCollector::CollectMessage(
    [[maybe_unused]] std::weak_ptr<CBryptPeer> const& wpBryptPeer,
    [[maybe_unused]] CMessageContext const& context,
    std::string_view buffer)
{
    m_pack = std::string(buffer.begin(), buffer.end());
    return true;
}

//------------------------------------------------------------------------------------------------

bool local::CMessageCollector::CollectMessage(
    [[maybe_unused]] std::weak_ptr<CBryptPeer> const& wpBryptPeer,
    [[maybe_unused]] CMessageContext const& context,
    [[maybe_unused]] Message::Buffer const& buffer)
{
    return false;
}

//------------------------------------------------------------------------------------------------
