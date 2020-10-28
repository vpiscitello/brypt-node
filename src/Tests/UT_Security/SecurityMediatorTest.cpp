//------------------------------------------------------------------------------------------------
#include "../../BryptIdentifier/BryptIdentifier.hpp"
#include "../../BryptMessage/HandshakeMessage.hpp"
#include "../../BryptMessage/MessageContext.hpp"
#include "../../BryptMessage/MessageDefinitions.hpp"
#include "../../Components/BryptPeer/BryptPeer.hpp"
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

BryptIdentifier::CContainer const ServerIdentifier(BryptIdentifier::Generate());
BryptIdentifier::CContainer const ClientIdentifier(BryptIdentifier::Generate());
constexpr std::string_view const Message = "Hello World!";

//------------------------------------------------------------------------------------------------
} // local namespace
} // namespace
//------------------------------------------------------------------------------------------------

class local::CStrategyStub : public ISecurityStrategy
{
public:
    CStrategyStub();

    virtual Security::Strategy GetStrategyType() const override;
    virtual Security::Role GetRole() const override;

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
        std::move(upStrategyStub), std::weak_ptr<IMessageSink>());

    auto const spBryptPeer = std::make_shared<CBryptPeer>(test::ClientIdentifier);
    upSecurityMediator->Bind(spBryptPeer);

    auto const optMessage = CHandshakeMessage::Builder()
        .SetSource(test::ServerIdentifier)
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
        std::move(upStrategyStub), spCollector);

    auto const spBryptPeer = std::make_shared<CBryptPeer>(test::ClientIdentifier);
    upSecurityMediator->Bind(spBryptPeer);

    auto const optMessage = CHandshakeMessage::Builder()
        .SetSource(test::ServerIdentifier)
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
        std::move(upStrategyStub), spCollector);

    auto const spBryptPeer = std::make_shared<CBryptPeer>(test::ClientIdentifier);
    upSecurityMediator->Bind(spBryptPeer);

    auto const optMessage = CHandshakeMessage::Builder()
        .SetSource(test::ServerIdentifier)
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

local::CStrategyStub::CStrategyStub()
{
}

//------------------------------------------------------------------------------------------------

Security::Strategy local::CStrategyStub::GetStrategyType() const
{
    return Security::Strategy::Invalid;
}

//------------------------------------------------------------------------------------------------

Security::Role local::CStrategyStub::GetRole() const
{
    return Security::Role::Initiator;
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
