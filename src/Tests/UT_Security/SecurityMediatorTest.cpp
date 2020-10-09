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
    auto const spBryptPeer = std::make_shared<CBryptPeer>(test::ClientIdentifier);
    auto upSecurityMediator = std::make_unique<CSecurityMediator>(
        nullptr, std::weak_ptr<IMessageSink>());
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
    auto const spBryptPeer = std::make_shared<CBryptPeer>(test::ClientIdentifier);
    auto spCollector = std::make_shared<local::CMessageCollector>();
    auto upSecurityMediator = std::make_unique<CSecurityMediator>(nullptr, spCollector);
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
    EXPECT_EQ(upSecurityMediator->GetSecurityState(), SecurityState::Authorized);

    EXPECT_TRUE(spBryptPeer->ScheduleReceive({}, pack));

    // Verify the stub message sink received the message
    EXPECT_EQ(spCollector->GetCollectedPack(), pack);
}

//------------------------------------------------------------------------------------------------

TEST(SecurityMediatorSuite, FailedExchangeTest)
{
    auto const spBryptPeer = std::make_shared<CBryptPeer>(test::ClientIdentifier);
    auto spCollector = std::make_shared<local::CMessageCollector>();
    auto upSecurityMediator = std::make_unique<CSecurityMediator>(nullptr, spCollector);
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
    EXPECT_EQ(upSecurityMediator->GetSecurityState(), SecurityState::Unauthorized);

    EXPECT_FALSE(spBryptPeer->ScheduleReceive({}, pack));
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
