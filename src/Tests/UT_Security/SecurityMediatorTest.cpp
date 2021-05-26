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
#include "Components/Security/Mediator.hpp"
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

class ConnectProtocolStub;
class StrategyStub;
class MessageCollector;

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

class local::ConnectProtocolStub : public IConnectProtocol
{
public:
    ConnectProtocolStub();

    // IConnectProtocol {
    virtual bool SendRequest(
        Node::SharedIdentifier const& spSourceIdentifier,
        std::shared_ptr<Peer::Proxy> const& spPeerProxy,
        MessageContext const& context) const override;
    // } IConnectProtocol 

    bool CalledBy(Node::SharedIdentifier const& spNodeIdentifier) const;
    bool CalledOnce() const;

private:
    mutable std::vector<Node::Internal::Identifier::Type> m_callers;

};

//----------------------------------------------------------------------------------------------------------------------

class local::StrategyStub : public ISecurityStrategy
{
public:
    StrategyStub();

    virtual Security::Strategy GetStrategyType() const override;
    virtual Security::Role GetRoleType() const override;
    virtual Security::Context GetContextType() const override;
    virtual std::size_t GetSignatureSize() const override;

    virtual std::uint32_t GetSynchronizationStages() const override;
    virtual Security::SynchronizationStatus GetSynchronizationStatus() const override;
    virtual Security::SynchronizationResult PrepareSynchronization() override;
    virtual Security::SynchronizationResult Synchronize(Security::ReadableView) override;

    virtual Security::OptionalBuffer Encrypt(Security::ReadableView, std::uint64_t) const override;
    virtual Security::OptionalBuffer Decrypt(Security::ReadableView, std::uint64_t) const override;

    virtual std::int32_t Sign(Security::Buffer&) const override;
    virtual Security::VerificationStatus Verify(Security::ReadableView) const override;

private: 
    virtual std::int32_t Sign(Security::ReadableView, Security::Buffer&) const override;
    virtual Security::OptionalBuffer GenerateSignature(Security::ReadableView, Security::ReadableView) const override;
};

//----------------------------------------------------------------------------------------------------------------------

class local::MessageCollector : public IMessageSink
{
public:
    MessageCollector();

    // IMessageSink {
    virtual bool CollectMessage(
        std::weak_ptr<Peer::Proxy> const& wpPeerProxy,
        MessageContext const& context,
        std::string_view buffer) override;
        
    virtual bool CollectMessage(
        std::weak_ptr<Peer::Proxy> const& wpPeerProxy,
        MessageContext const& context,
        std::span<std::uint8_t const> buffer) override;
    // }IMessageSink
    
    std::string GetCollectedPack() const;
    std::string GetCollectedData() const;

private:
    std::string m_pack;
    std::string m_data;

};

//----------------------------------------------------------------------------------------------------------------------

TEST(SecurityMediatorSuite, ExchangeProcessorLifecycleTest)
{
    auto upStrategyStub = std::make_unique<local::StrategyStub>();
    auto upSecurityMediator = std::make_unique<Security::Mediator>(
        test::ServerIdentifier, Security::Context::Unique, std::weak_ptr<IMessageSink>());

    EXPECT_TRUE(upSecurityMediator->SetupExchangeProcessor(std::move(upStrategyStub), nullptr));    

    auto const spPeerProxy = std::make_shared<Peer::Proxy>(*test::ClientIdentifier);
    upSecurityMediator->BindPeer(spPeerProxy);

    Peer::Registration registration(test::EndpointIdentifier, test::EndpointProtocol, test::RemoteClientAddress, {});
    upSecurityMediator->BindSecurityContext(registration.GetWritableMessageContext());
    spPeerProxy->RegisterSilentEndpoint<InvokeContext::Test>(registration);

    auto const optMessage = NetworkMessage::Builder()
        .SetSource(*test::ServerIdentifier)
        .MakeHandshakeMessage()
        .SetPayload(test::HandshakeMessage)
        .ValidatedBuild();
    ASSERT_TRUE(optMessage);

    std::string const pack = optMessage->GetPack();

    EXPECT_TRUE(spPeerProxy->ScheduleReceive(test::EndpointIdentifier, pack));

    // Verify the node can't forward a message through the receiver, because it has been unset by the mediator. 
    upSecurityMediator.reset();
    EXPECT_FALSE(spPeerProxy->ScheduleReceive(test::EndpointIdentifier, pack));
}

//----------------------------------------------------------------------------------------------------------------------

TEST(SecurityMediatorSuite, SuccessfulExchangeTest)
{
    auto upStrategyStub = std::make_unique<local::StrategyStub>();
    auto spCollector = std::make_shared<local::MessageCollector>();
    auto upSecurityMediator = std::make_unique<Security::Mediator>(
        test::ServerIdentifier, Security::Context::Unique, spCollector);

    EXPECT_TRUE(upSecurityMediator->SetupExchangeProcessor(std::move(upStrategyStub), nullptr));

    auto const spPeerProxy = std::make_shared<Peer::Proxy>(*test::ClientIdentifier);
    upSecurityMediator->BindPeer(spPeerProxy);

    Peer::Registration registration(test::EndpointIdentifier, test::EndpointProtocol, test::RemoteClientAddress, {});
    upSecurityMediator->BindSecurityContext(registration.GetWritableMessageContext());
    spPeerProxy->RegisterSilentEndpoint<InvokeContext::Test>(registration);

    auto const optMessage = NetworkMessage::Builder()
        .SetSource(*test::ServerIdentifier)
        .MakeHandshakeMessage()
        .SetPayload(test::HandshakeMessage)
        .ValidatedBuild();
    ASSERT_TRUE(optMessage);

    std::string const pack = optMessage->GetPack();

    EXPECT_TRUE(spPeerProxy->ScheduleReceive(test::EndpointIdentifier, pack));

    // Verify the peer's receiver has been swapped to the stub message sink when the mediator is notified of a
    // sucessful exchange. 
    upSecurityMediator->OnExchangeClose(ExchangeStatus::Success);
    EXPECT_EQ(upSecurityMediator->GetSecurityState(), Security::State::Authorized);

    EXPECT_TRUE(spPeerProxy->ScheduleReceive(test::EndpointIdentifier, pack));

    // Verify the stub message sink received the message
    EXPECT_EQ(spCollector->GetCollectedPack(), pack);
}

//----------------------------------------------------------------------------------------------------------------------

TEST(SecurityMediatorSuite, FailedExchangeTest)
{
    auto upStrategyStub = std::make_unique<local::StrategyStub>();
    auto spCollector = std::make_shared<local::MessageCollector>();
    auto upSecurityMediator = std::make_unique<Security::Mediator>(
        test::ServerIdentifier, Security::Context::Unique, spCollector);

    EXPECT_TRUE(upSecurityMediator->SetupExchangeProcessor(std::move(upStrategyStub), nullptr));

    auto const spPeerProxy = std::make_shared<Peer::Proxy>(*test::ClientIdentifier);
    upSecurityMediator->BindPeer(spPeerProxy);

    Peer::Registration registration(test::EndpointIdentifier, test::EndpointProtocol, {});
    upSecurityMediator->BindSecurityContext(registration.GetWritableMessageContext());
    spPeerProxy->RegisterSilentEndpoint<InvokeContext::Test>(registration);
    
    auto const optMessage = NetworkMessage::Builder()
        .SetSource(*test::ServerIdentifier)
        .MakeHandshakeMessage()
        .SetPayload(test::HandshakeMessage)
        .ValidatedBuild();
    ASSERT_TRUE(optMessage);

    std::string const pack = optMessage->GetPack();

    EXPECT_TRUE(spPeerProxy->ScheduleReceive(test::EndpointIdentifier, pack));

    // Verify the peer receiver has been dropped when the tracker has been notified of a failed exchange. 
    upSecurityMediator->OnExchangeClose(ExchangeStatus::Failed);
    EXPECT_EQ(upSecurityMediator->GetSecurityState(), Security::State::Unauthorized);

    EXPECT_FALSE(spPeerProxy->ScheduleReceive(test::EndpointIdentifier, pack));
}

//----------------------------------------------------------------------------------------------------------------------

TEST(SecurityMediatorSuite, PQNISTL3SuccessfulExchangeTest)
{
    // Declare the server resoucres for the test. 
    std::shared_ptr<Peer::Proxy> spServerPeer;
    std::unique_ptr<Security::Mediator> upServerMediator;

    // Declare the client resources for the test. 
    std::shared_ptr<Peer::Proxy> spClientPeer;
    std::unique_ptr<Security::Mediator> upClientMediator;

    auto const spConnectProtocol = std::make_shared<local::ConnectProtocolStub>();
    auto const spCollector = std::make_shared<local::MessageCollector>();

    // Setup the client's view of the mediator.
    {
        upClientMediator = std::make_unique<Security::Mediator>(
            test::ClientIdentifier, Security::Context::Unique, spCollector);

        spClientPeer = std::make_shared<Peer::Proxy>(*test::ServerIdentifier);

        Peer::Registration registration(
            test::EndpointIdentifier,
            test::EndpointProtocol,
            test::RemoteServerAddress,
            [&spServerPeer] ([[maybe_unused]] auto const& destination, auto&& message) -> bool
            {
                return spServerPeer->ScheduleReceive(test::EndpointIdentifier, std::get<std::string>(message));
            });

        upClientMediator->BindSecurityContext(registration.GetWritableMessageContext());
        spClientPeer->RegisterSilentEndpoint<InvokeContext::Test>(registration);
    }

    // Setup the servers's view of the exchange.
    {
        upServerMediator = std::make_unique<Security::Mediator>(
            test::ServerIdentifier, Security::Context::Unique, spCollector);

        spServerPeer = std::make_shared<Peer::Proxy>(*test::ClientIdentifier);

        Peer::Registration registration(
            test::EndpointIdentifier,
            test::EndpointProtocol,
            test::RemoteClientAddress,
            [&spClientPeer] ([[maybe_unused]] auto const& destination, auto&& message) -> bool
            {
                return spClientPeer->ScheduleReceive(test::EndpointIdentifier, std::get<std::string>(message));
            });

        upServerMediator->BindSecurityContext(registration.GetWritableMessageContext());
        spServerPeer->RegisterSilentEndpoint<InvokeContext::Test>(registration);
    }

    // Setup an exchange through the mediator as the initiator
    {
        auto optRequest = upClientMediator->SetupExchangeInitiator(Security::Strategy::PQNISTL3, spConnectProtocol);
        ASSERT_TRUE(optRequest);
        upClientMediator->BindPeer(spClientPeer); // Bind the client mediator to the client peer. 
        
        // Setup an exchange through the mediator as the acceptor. 
        EXPECT_TRUE(upServerMediator->SetupExchangeAcceptor(Security::Strategy::PQNISTL3));
        upServerMediator->BindPeer(spServerPeer); // Bind the server mediator to the server peer. 

        EXPECT_TRUE(spClientPeer->ScheduleSend(test::EndpointIdentifier, std::move(*optRequest)));
    }

    // We expect that the connect protocol has been used once. 
    EXPECT_TRUE(spConnectProtocol->CalledOnce());

    EXPECT_EQ(upClientMediator->GetSecurityState(), Security::State::Authorized);
    EXPECT_TRUE(spConnectProtocol->CalledBy(test::ClientIdentifier));
    EXPECT_EQ(spClientPeer->GetSentCount(), Security::PQNISTL3::Strategy::AcceptorStages + 1);

    EXPECT_EQ(upServerMediator->GetSecurityState(), Security::State::Authorized);
    EXPECT_FALSE(spConnectProtocol->CalledBy(test::ServerIdentifier));
    EXPECT_EQ(spServerPeer->GetSentCount(), Security::PQNISTL3::Strategy::InitiatorStages);

    EXPECT_EQ(spCollector->GetCollectedData(), test::ConnectMessage);

    auto const optClientContext = spClientPeer->GetMessageContext(test::EndpointIdentifier);
    ASSERT_TRUE(optClientContext);

    auto const optApplicationRequest = ApplicationMessage::Builder()
        .SetMessageContext(*optClientContext)
        .SetSource(*test::ClientIdentifier)
        .SetDestination(*test::ServerIdentifier)
        .SetCommand(Handler::Type::Information, 0)
        .SetPayload("Information Request")
        .ValidatedBuild();
    ASSERT_TRUE(optApplicationRequest);

    std::string const request = optApplicationRequest->GetPack();
    EXPECT_TRUE(spClientPeer->ScheduleSend(test::EndpointIdentifier, std::string(request)));
    EXPECT_EQ(spCollector->GetCollectedPack(), request);

    auto const optServerContext = spServerPeer->GetMessageContext(test::EndpointIdentifier);
    ASSERT_TRUE(optServerContext);

    auto const optApplicationResponse = ApplicationMessage::Builder()
        .SetMessageContext(*optServerContext)
        .SetSource(*test::ClientIdentifier)
        .SetDestination(*test::ServerIdentifier)
        .SetCommand(Handler::Type::Information, 1)
        .SetPayload("Information Response")
        .ValidatedBuild();
    ASSERT_TRUE(optApplicationResponse);

    std::string const response = optApplicationResponse->GetPack();
    EXPECT_TRUE(spServerPeer->ScheduleSend(test::EndpointIdentifier, std::string(response)));
    EXPECT_EQ(spCollector->GetCollectedPack(), response);
}

//----------------------------------------------------------------------------------------------------------------------

local::ConnectProtocolStub::ConnectProtocolStub()
    : m_callers()
{
}

//----------------------------------------------------------------------------------------------------------------------

bool local::ConnectProtocolStub::SendRequest(
    Node::SharedIdentifier const& spSourceIdentifier,
    std::shared_ptr<Peer::Proxy> const& spPeerProxy,
    MessageContext const& context) const
{
    if (!spSourceIdentifier || !spPeerProxy) {
        return false;
    }

    m_callers.emplace_back(spSourceIdentifier->GetInternalValue());

    auto const optConnectRequest = ApplicationMessage::Builder()
        .SetMessageContext(context)
        .SetSource(*test::ClientIdentifier)
        .SetDestination(*test::ServerIdentifier)
        .SetCommand(Handler::Type::Connect, 0)
        .SetPayload(test::ConnectMessage)
        .ValidatedBuild();
    assert(optConnectRequest);

    return spPeerProxy->ScheduleSend(
        context.GetEndpointIdentifier(), optConnectRequest->GetPack());
}

//----------------------------------------------------------------------------------------------------------------------

bool local::ConnectProtocolStub::CalledBy(
    Node::SharedIdentifier const& spNodeIdentifier) const
{
    auto const itr = std::find(
        m_callers.begin(), m_callers.end(), spNodeIdentifier->GetInternalValue());

    return (itr != m_callers.end());
}

//----------------------------------------------------------------------------------------------------------------------

bool local::ConnectProtocolStub::CalledOnce() const
{
    return m_callers.size();
}

//----------------------------------------------------------------------------------------------------------------------

local::StrategyStub::StrategyStub()
{
}

//----------------------------------------------------------------------------------------------------------------------

Security::Strategy local::StrategyStub::GetStrategyType() const
{
    return Security::Strategy::Invalid;
}

//----------------------------------------------------------------------------------------------------------------------

Security::Role local::StrategyStub::GetRoleType() const
{
    return Security::Role::Initiator;
}

//----------------------------------------------------------------------------------------------------------------------

Security::Context local::StrategyStub::GetContextType() const
{
    return Security::Context::Unique;
}

//----------------------------------------------------------------------------------------------------------------------

std::size_t local::StrategyStub::GetSignatureSize() const
{
    return 0;
}

//----------------------------------------------------------------------------------------------------------------------

std::uint32_t local::StrategyStub::GetSynchronizationStages() const
{
    return 0;
}

//----------------------------------------------------------------------------------------------------------------------

Security::SynchronizationStatus local::StrategyStub::GetSynchronizationStatus() const
{
    return Security::SynchronizationStatus::Processing;
}

//----------------------------------------------------------------------------------------------------------------------

Security::SynchronizationResult local::StrategyStub::PrepareSynchronization()
{
    return { Security::SynchronizationStatus::Processing, {} };
}

//----------------------------------------------------------------------------------------------------------------------

Security::SynchronizationResult local::StrategyStub::Synchronize(
   [[maybe_unused]]  Security::ReadableView)
{
    return { Security::SynchronizationStatus::Processing, {} };
}

//----------------------------------------------------------------------------------------------------------------------

Security::OptionalBuffer local::StrategyStub::Encrypt(
    [[maybe_unused]] Security::ReadableView,
    [[maybe_unused]] std::uint64_t) const
{
    return {};
}

//----------------------------------------------------------------------------------------------------------------------

Security::OptionalBuffer local::StrategyStub::Decrypt(
    [[maybe_unused]] Security::ReadableView,
    [[maybe_unused]] std::uint64_t) const
{
    return {};
}


//----------------------------------------------------------------------------------------------------------------------

std::int32_t local::StrategyStub::Sign(
    [[maybe_unused]] Security::Buffer&) const
{
    return 0;
}

//----------------------------------------------------------------------------------------------------------------------

Security::VerificationStatus local::StrategyStub::Verify(
    [[maybe_unused]] Security::ReadableView) const
{
    return Security::VerificationStatus::Failed;
}

//----------------------------------------------------------------------------------------------------------------------

std::int32_t local::StrategyStub::Sign(
    [[maybe_unused]] Security::ReadableView, [[maybe_unused]] Security::Buffer&) const
{
    return 0;
}

//----------------------------------------------------------------------------------------------------------------------

Security::OptionalBuffer local::StrategyStub::GenerateSignature(
    [[maybe_unused]] Security::ReadableView,
    [[maybe_unused]] Security::ReadableView) const
{
    return {};
}

//----------------------------------------------------------------------------------------------------------------------

local::MessageCollector::MessageCollector()
    : m_pack()
    , m_data()
{
}

//----------------------------------------------------------------------------------------------------------------------

bool local::MessageCollector::CollectMessage(
    [[maybe_unused]] std::weak_ptr<Peer::Proxy> const& wpPeerProxy,
    MessageContext const& context,
    std::string_view buffer)
{
    m_pack = std::string(buffer.begin(), buffer.end());

    auto decoded = Z85::Decode(buffer);
    auto const optProtocol = Message::PeekProtocol(decoded);
    if (!optProtocol) {
        return false;
    }

    switch (*optProtocol) {
        case Message::Protocol::Application: {
            auto const optMessage = ApplicationMessage::Builder()
                .SetMessageContext(context)
                .FromDecodedPack(decoded)
                .ValidatedBuild();
            assert(optMessage);

            auto const payload = optMessage->GetPayload();
            m_data = std::string(payload.begin(), payload.end());
        } break;
        default: break;
    }

    return true;
}

//----------------------------------------------------------------------------------------------------------------------

bool local::MessageCollector::CollectMessage(
    [[maybe_unused]] std::weak_ptr<Peer::Proxy> const& wpPeerProxy,
    [[maybe_unused]] MessageContext const& context,
    [[maybe_unused]] std::span<std::uint8_t const> buffer)
{
    return false;
}

//----------------------------------------------------------------------------------------------------------------------

std::string local::MessageCollector::GetCollectedPack() const
{
    return m_pack;
}

//----------------------------------------------------------------------------------------------------------------------

std::string local::MessageCollector::GetCollectedData() const
{
    return m_data;
}

//----------------------------------------------------------------------------------------------------------------------
