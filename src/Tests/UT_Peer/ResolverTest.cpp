//----------------------------------------------------------------------------------------------------------------------
#include "BryptIdentifier/BryptIdentifier.hpp"
#include "BryptMessage/ApplicationMessage.hpp"
#include "BryptMessage/NetworkMessage.hpp"
#include "BryptMessage/MessageContext.hpp"
#include "BryptMessage/MessageDefinitions.hpp"
#include "BryptMessage/MessageUtils.hpp"
#include "Components/Network/EndpointIdentifier.hpp"
#include "Components/Network/Protocol.hpp"
#include "Components/Peer/Proxy.hpp"
#include "Components/Peer/Resolver.hpp"
#include "Components/Security/PostQuantum/NISTSecurityLevelThree.hpp"
#include "Interfaces/ConnectProtocol.hpp"
#include "Interfaces/ExchangeObserver.hpp"
#include "Interfaces/MessageSink.hpp"
#include "Interfaces/SecurityStrategy.hpp"
#include "Utilities/Z85.hpp"
#include "Utilities/InvokeContext.hpp"
//----------------------------------------------------------------------------------------------------------------------
#include <gtest/gtest.h>
//----------------------------------------------------------------------------------------------------------------------
#include <cassert>
#include <memory>
//----------------------------------------------------------------------------------------------------------------------

//----------------------------------------------------------------------------------------------------------------------
namespace {
namespace local {
//----------------------------------------------------------------------------------------------------------------------

class StrategyStub;
class ProcessorStub;

void CloseExchange(Peer::Resolver* pResolver, ExchangeStatus status);

//----------------------------------------------------------------------------------------------------------------------
} // local namespace
//----------------------------------------------------------------------------------------------------------------------
namespace test {
//----------------------------------------------------------------------------------------------------------------------

auto const ClientIdentifier = std::make_shared<Node::Identifier>(Node::GenerateIdentifier());
auto const ServerIdentifier = std::make_shared<Node::Identifier>(Node::GenerateIdentifier());
    
constexpr std::string_view const HandshakeMessage = "Handshake Request";
constexpr std::string_view const ConnectMessage = "Connection Request";

constexpr Network::Endpoint::Identifier const EndpointIdentifier = 1;
constexpr Network::Protocol const EndpointProtocol = Network::Protocol::TCP;

Network::RemoteAddress const RemoteServerAddress(Network::Protocol::TCP, "127.0.0.1:35216", true);
Network::RemoteAddress const RemoteClientAddress(Network::Protocol::TCP, "127.0.0.1:35217", false);

//----------------------------------------------------------------------------------------------------------------------
} // local namespace
} // namespace
//----------------------------------------------------------------------------------------------------------------------

class local::StrategyStub : public ISecurityStrategy
{
public:
    StrategyStub() { }

    virtual Security::Strategy GetStrategyType() const override { return Security::Strategy::Invalid; }
    virtual Security::Role GetRoleType() const override { return Security::Role::Initiator; }
    virtual Security::Context GetContextType() const override { return Security::Context::Unique; }
    virtual std::size_t GetSignatureSize() const override { return 0; }

    virtual std::uint32_t GetSynchronizationStages() const override { return 0; }
    virtual Security::SynchronizationStatus GetSynchronizationStatus() const override
        { return Security::SynchronizationStatus::Processing; }
    virtual Security::SynchronizationResult PrepareSynchronization() override
        { return { Security::SynchronizationStatus::Processing, {} }; }
    virtual Security::SynchronizationResult Synchronize(Security::ReadableView) override
        { return { Security::SynchronizationStatus::Processing, {} }; }

    virtual Security::OptionalBuffer Encrypt(Security::ReadableView, std::uint64_t) const override { return {}; }
    virtual Security::OptionalBuffer Decrypt(Security::ReadableView, std::uint64_t) const override { return {}; }

    virtual std::int32_t Sign(Security::Buffer&) const override { return 0; }
    virtual Security::VerificationStatus Verify(Security::ReadableView) const override
        { return Security::VerificationStatus::Failed; }

private: 
    virtual std::int32_t Sign(Security::ReadableView, Security::Buffer&) const override { return 0; }
    virtual Security::OptionalBuffer GenerateSignature(Security::ReadableView, Security::ReadableView) const override
        { return {}; }
};

//----------------------------------------------------------------------------------------------------------------------

class local::ProcessorStub : public IMessageSink
{
public:
    ProcessorStub() : m_pack() { }

    // IMessageSink {
    virtual bool CollectMessage(
        std::weak_ptr<Peer::Proxy> const&, MessageContext const&, std::string_view buffer) override
        { m_pack = std::string(buffer.begin(), buffer.end()); return true; }
        
    virtual bool CollectMessage(
        std::weak_ptr<Peer::Proxy> const&, MessageContext const&, std::span<std::uint8_t const>) override
        { return false; }
    // } IMessageSink
    
    std::string GetCollectedPack() const { return m_pack; }

private:
    std::string m_pack;
};

//----------------------------------------------------------------------------------------------------------------------

TEST(PeerResolverSuite, ExchangeProcessorLifecycleTest)
{
    auto const spProxy = std::make_shared<Peer::Proxy>(*test::ClientIdentifier);
    
    {
        auto upStrategyStub = std::make_unique<local::StrategyStub>();
        auto upResolver = std::make_unique<Peer::Resolver>(test::ServerIdentifier, Security::Context::Unique);
        ASSERT_TRUE(upResolver->SetupTestProcessor<InvokeContext::Test>(std::move(upStrategyStub)));
        ASSERT_TRUE(spProxy->AttachResolver(std::move(upResolver)));
    }

    Peer::Registration registration(test::EndpointIdentifier, test::EndpointProtocol, test::RemoteClientAddress, {});
    spProxy->RegisterSilentEndpoint<InvokeContext::Test>(registration);

    auto const optMessage = NetworkMessage::Builder()
        .SetSource(*test::ServerIdentifier)
        .MakeHandshakeMessage()
        .SetPayload(test::HandshakeMessage)
        .ValidatedBuild();
    ASSERT_TRUE(optMessage);

    std::string const pack = optMessage->GetPack();
    EXPECT_TRUE(spProxy->ScheduleReceive(test::EndpointIdentifier, pack));

    // Verify the node can't forward a message through the receiver, because it has been unset by the mediator. 
    spProxy->DetachResolver<InvokeContext::Test>();
    EXPECT_FALSE(spProxy->ScheduleReceive(test::EndpointIdentifier, pack));
}

//----------------------------------------------------------------------------------------------------------------------

TEST(PeerResolverSuite, SuccessfulExchangeTest)
{
    auto const spCollector = std::make_shared<local::ProcessorStub>();
    auto const spProxy = std::make_shared<Peer::Proxy>(*test::ClientIdentifier, nullptr, spCollector);
    Peer::Resolver* pCapturedResolver = nullptr;
    {
        auto upStrategyStub = std::make_unique<local::StrategyStub>();
        auto upResolver = std::make_unique<Peer::Resolver>(test::ServerIdentifier, Security::Context::Unique);
        pCapturedResolver = upResolver.get();
        ASSERT_TRUE(upResolver->SetupTestProcessor<InvokeContext::Test>(std::move(upStrategyStub)));
        ASSERT_TRUE(spProxy->AttachResolver(std::move(upResolver)));
    }

    Peer::Registration registration(test::EndpointIdentifier, test::EndpointProtocol, test::RemoteClientAddress, {});
    spProxy->RegisterSilentEndpoint<InvokeContext::Test>(registration);

    auto const optMessage = NetworkMessage::Builder()
        .SetSource(*test::ServerIdentifier)
        .MakeHandshakeMessage()
        .SetPayload(test::HandshakeMessage)
        .ValidatedBuild();
    ASSERT_TRUE(optMessage);

    std::string const pack = optMessage->GetPack();
    EXPECT_TRUE(spProxy->ScheduleReceive(test::EndpointIdentifier, pack));

    // Verify the receiver is swapped to the authorized processor when resolver is notified of a sucessful exchange. 
    local::CloseExchange(pCapturedResolver, ExchangeStatus::Success);
    EXPECT_EQ(spProxy->GetSecurityState(), Security::State::Authorized);
    EXPECT_TRUE(spProxy->ScheduleReceive(test::EndpointIdentifier, pack));
    EXPECT_EQ(spCollector->GetCollectedPack(), pack);  // Verify the stub message sink received the message
}

//----------------------------------------------------------------------------------------------------------------------

TEST(PeerResolverSuite, FailedExchangeTest)
{
    auto spCollector = std::make_shared<local::ProcessorStub>();
    auto const spProxy = std::make_shared<Peer::Proxy>(*test::ClientIdentifier, nullptr, spCollector);
    Peer::Resolver* pCapturedResolver = nullptr;
    {
        auto upStrategyStub = std::make_unique<local::StrategyStub>();
        auto upResolver = std::make_unique<Peer::Resolver>(test::ServerIdentifier, Security::Context::Unique);
        pCapturedResolver = upResolver.get();
        ASSERT_TRUE(upResolver->SetupTestProcessor<InvokeContext::Test>(std::move(upStrategyStub)));
        ASSERT_TRUE(spProxy->AttachResolver(std::move(upResolver)));
    }

    Peer::Registration registration(test::EndpointIdentifier, test::EndpointProtocol, {});
    spProxy->RegisterSilentEndpoint<InvokeContext::Test>(registration);
    
    auto const optMessage = NetworkMessage::Builder()
        .SetSource(*test::ServerIdentifier)
        .MakeHandshakeMessage()
        .SetPayload(test::HandshakeMessage)
        .ValidatedBuild();
    ASSERT_TRUE(optMessage);

    std::string const pack = optMessage->GetPack();

    EXPECT_TRUE(spProxy->ScheduleReceive(test::EndpointIdentifier, pack));

    // Verify the peer receiver is dropped when the resolver has been notified of a failed exchange. 
    local::CloseExchange(pCapturedResolver, ExchangeStatus::Failed);
    EXPECT_EQ(spProxy->GetSecurityState(), Security::State::Unauthorized);
    EXPECT_FALSE(spProxy->ScheduleReceive(test::EndpointIdentifier, pack));
}

//----------------------------------------------------------------------------------------------------------------------

void local::CloseExchange(Peer::Resolver* pResolver, ExchangeStatus status)
{
    pResolver->OnExchangeClose(status); // Note: the resolver will be destroyed by the peer after this call. 
    pResolver = nullptr; // Ensure the captured resolver is not dangling. 
}

//----------------------------------------------------------------------------------------------------------------------
