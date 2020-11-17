//------------------------------------------------------------------------------------------------
#include "../../BryptIdentifier/BryptIdentifier.hpp"
#include "../../BryptMessage/ApplicationMessage.hpp"
#include "../../BryptMessage/NetworkMessage.hpp"
#include "../../BryptMessage/MessageContext.hpp"
#include "../../BryptMessage/MessageDefinitions.hpp"
#include "../../BryptMessage/MessageUtils.hpp"
#include "../../BryptMessage/PackUtils.hpp"
#include "../../Components/BryptPeer/BryptPeer.hpp"
#include "../../Components/Endpoints/EndpointIdentifier.hpp"
#include "../../Components/Endpoints/TechnologyType.hpp"
#include "../../Components/Security/SecurityMediator.hpp"
#include "../../Components/Security/PostQuantum/NISTSecurityLevelThree.hpp"
#include "../../Interfaces/ConnectProtocol.hpp"
#include "../../Interfaces/ExchangeObserver.hpp"
#include "../../Interfaces/MessageSink.hpp"
#include "../../Interfaces/SecurityStrategy.hpp"
//------------------------------------------------------------------------------------------------
#include "../../Libraries/googletest/include/gtest/gtest.h"
//------------------------------------------------------------------------------------------------
#include <cassert>
#include <memory>
//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
namespace {
namespace local {
//------------------------------------------------------------------------------------------------

class CConnectProtocolStub;
class CStrategyStub;
class CMessageCollector;

//------------------------------------------------------------------------------------------------
} // local namespace
//------------------------------------------------------------------------------------------------
namespace test {
//------------------------------------------------------------------------------------------------

auto const ClientIdentifier = std::make_shared<BryptIdentifier::CContainer>(
    BryptIdentifier::Generate());
auto const ServerIdentifier = std::make_shared<BryptIdentifier::CContainer>(
    BryptIdentifier::Generate());
    
constexpr std::string_view const HandshakeMessage = "Handshake Request";
constexpr std::string_view const ConnectMessage = "Connection Request";

constexpr Endpoints::EndpointIdType const EndpointIdentifier = 1;
constexpr Endpoints::TechnologyType const EndpointTechnology = Endpoints::TechnologyType::TCP;

//------------------------------------------------------------------------------------------------
} // local namespace
} // namespace
//------------------------------------------------------------------------------------------------

class local::CConnectProtocolStub : public IConnectProtocol
{
public:
    CConnectProtocolStub();

    // IConnectProtocol {
    virtual bool SendRequest(
        BryptIdentifier::SharedContainer const& spSourceIdentifier,
        std::shared_ptr<CBryptPeer> const& spBryptPeer,
        CMessageContext const& context) const override;
    // } IConnectProtocol 

    bool CalledBy(BryptIdentifier::SharedContainer const& spBryptIdentifier) const;
    bool CalledOnce() const;

private:
    mutable std::vector<BryptIdentifier::Internal::Type> m_callers;

};

//------------------------------------------------------------------------------------------------

class local::CStrategyStub : public ISecurityStrategy
{
public:
    CStrategyStub();

    virtual Security::Strategy GetStrategyType() const override;
    virtual Security::Role GetRoleType() const override;
    virtual Security::Context GetContextType() const override;
    virtual std::uint32_t GetSignatureSize() const override;

    virtual std::uint32_t GetSynchronizationStages() const override;
    virtual Security::SynchronizationStatus GetSynchronizationStatus() const override;
    virtual Security::SynchronizationResult PrepareSynchronization() override;
    virtual Security::SynchronizationResult Synchronize(Security::Buffer const&) override;

    virtual Security::OptionalBuffer Encrypt(
        Security::Buffer const&, std::uint32_t, std::uint64_t) const override;
    virtual Security::OptionalBuffer Decrypt(
        Security::Buffer const&, std::uint32_t, std::uint64_t) const override;

    virtual std::int32_t Sign(Security::Buffer&) const override;
    virtual Security::VerificationStatus Verify(Security::Buffer const&) const override;

private: 
    virtual std::int32_t Sign(
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
    
    std::string GetCollectedPack() const;
    std::string GetCollectedData() const;

private:
    std::string m_pack;
    std::string m_data;

};

//------------------------------------------------------------------------------------------------

TEST(SecurityMediatorSuite, ExchangeProcessorLifecycleTest)
{
    auto upStrategyStub = std::make_unique<local::CStrategyStub>();
    auto upSecurityMediator = std::make_unique<CSecurityMediator>(
        test::ServerIdentifier, Security::Context::Unique, std::weak_ptr<IMessageSink>());

    upSecurityMediator->SetupExchangeProcessor(std::move(upStrategyStub), nullptr);    

    auto const spBryptPeer = std::make_shared<CBryptPeer>(*test::ClientIdentifier);
    upSecurityMediator->BindPeer(spBryptPeer);

    CEndpointRegistration registration(
        test::EndpointIdentifier, test::EndpointTechnology, {});
    upSecurityMediator->BindSecurityContext(registration.GetWritableMessageContext());
    spBryptPeer->RegisterEndpoint(registration);

    auto const optMessage = CNetworkMessage::Builder()
        .SetSource(*test::ServerIdentifier)
        .MakeHandshakeMessage()
        .SetPayload(test::HandshakeMessage)
        .ValidatedBuild();
    ASSERT_TRUE(optMessage);

    std::string const pack = optMessage->GetPack();

    EXPECT_TRUE(spBryptPeer->ScheduleReceive(test::EndpointIdentifier, pack));

    // Verify the node can't forward a message through the receiver, because it has been 
    // unset by the CSecurityMediator. 
    upSecurityMediator.reset();
    EXPECT_FALSE(spBryptPeer->ScheduleReceive(test::EndpointIdentifier, pack));
}

//------------------------------------------------------------------------------------------------

TEST(SecurityMediatorSuite, SuccessfulExchangeTest)
{
    auto upStrategyStub = std::make_unique<local::CStrategyStub>();
    auto spCollector = std::make_shared<local::CMessageCollector>();
    auto upSecurityMediator = std::make_unique<CSecurityMediator>(
        test::ServerIdentifier, Security::Context::Unique, spCollector);

    upSecurityMediator->SetupExchangeProcessor(std::move(upStrategyStub), nullptr);

    auto const spBryptPeer = std::make_shared<CBryptPeer>(*test::ClientIdentifier);
    upSecurityMediator->BindPeer(spBryptPeer);

    CEndpointRegistration registration(
        test::EndpointIdentifier, test::EndpointTechnology, {});
    upSecurityMediator->BindSecurityContext(registration.GetWritableMessageContext());
    spBryptPeer->RegisterEndpoint(registration);

    auto const optMessage = CNetworkMessage::Builder()
        .SetSource(*test::ServerIdentifier)
        .MakeHandshakeMessage()
        .SetPayload(test::HandshakeMessage)
        .ValidatedBuild();
    ASSERT_TRUE(optMessage);

    std::string const pack = optMessage->GetPack();

    EXPECT_TRUE(spBryptPeer->ScheduleReceive(test::EndpointIdentifier, pack));

    // Verify the peer's receiver has been swapped to the stub message sink when the 
    // mediator is notified of a sucessful exchange. 
    upSecurityMediator->HandleExchangeClose(ExchangeStatus::Success);
    EXPECT_EQ(upSecurityMediator->GetSecurityState(), Security::State::Authorized);

    EXPECT_TRUE(spBryptPeer->ScheduleReceive(test::EndpointIdentifier, pack));

    // Verify the stub message sink received the message
    EXPECT_EQ(spCollector->GetCollectedPack(), pack);
}

//------------------------------------------------------------------------------------------------

TEST(SecurityMediatorSuite, FailedExchangeTest)
{
    auto upStrategyStub = std::make_unique<local::CStrategyStub>();
    auto spCollector = std::make_shared<local::CMessageCollector>();
    auto upSecurityMediator = std::make_unique<CSecurityMediator>(
        test::ServerIdentifier, Security::Context::Unique, spCollector);

    upSecurityMediator->SetupExchangeProcessor(std::move(upStrategyStub), nullptr);

    auto const spBryptPeer = std::make_shared<CBryptPeer>(*test::ClientIdentifier);
    upSecurityMediator->BindPeer(spBryptPeer);

    CEndpointRegistration registration(
        test::EndpointIdentifier, test::EndpointTechnology, {});
    upSecurityMediator->BindSecurityContext(registration.GetWritableMessageContext());
    spBryptPeer->RegisterEndpoint(registration);
    
    auto const optMessage = CNetworkMessage::Builder()
        .SetSource(*test::ServerIdentifier)
        .MakeHandshakeMessage()
        .SetPayload(test::HandshakeMessage)
        .ValidatedBuild();
    ASSERT_TRUE(optMessage);

    std::string const pack = optMessage->GetPack();

    EXPECT_TRUE(spBryptPeer->ScheduleReceive(test::EndpointIdentifier, pack));

    // Verify the peer receiver has been dropped when the tracker has been notified of a failed
    // exchange. 
    upSecurityMediator->HandleExchangeClose(ExchangeStatus::Failed);
    EXPECT_EQ(upSecurityMediator->GetSecurityState(), Security::State::Unauthorized);

    EXPECT_FALSE(spBryptPeer->ScheduleReceive(test::EndpointIdentifier, pack));
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

    auto const spConnectProtocol = std::make_shared<local::CConnectProtocolStub>();
    auto const spCollector = std::make_shared<local::CMessageCollector>();

    // Setup the client's view of the mediator.
    {
        upClientMediator = std::make_unique<CSecurityMediator>(
            test::ClientIdentifier, Security::Context::Unique, spCollector);

        spClientPeer = std::make_shared<CBryptPeer>(*test::ServerIdentifier);

        CEndpointRegistration registration(
            test::EndpointIdentifier,
            test::EndpointTechnology,
            [&spServerPeer] (
                [[maybe_unused]] auto const& destination, std::string_view message) -> bool
            {
                return spServerPeer->ScheduleReceive(test::EndpointIdentifier, message);
            });

        upClientMediator->BindSecurityContext(registration.GetWritableMessageContext());
        spClientPeer->RegisterEndpoint(registration);
    }

    // Setup the servers's view of the exchange.
    {
        upServerMediator = std::make_unique<CSecurityMediator>(
            test::ServerIdentifier, Security::Context::Unique, spCollector);

        spServerPeer = std::make_shared<CBryptPeer>(*test::ClientIdentifier);

        CEndpointRegistration registration(
            test::EndpointIdentifier,
            test::EndpointTechnology,
            [&spClientPeer] (
                [[maybe_unused]] auto const& destination, std::string_view message) -> bool
            {
                return spClientPeer->ScheduleReceive(test::EndpointIdentifier, message);
            });

        upServerMediator->BindSecurityContext(registration.GetWritableMessageContext());
        spServerPeer->RegisterEndpoint(registration);
    }

    // Setup an exchange through the mediator as the initiator
    auto const optRequest = upClientMediator->SetupExchangeInitiator(
        Security::Strategy::PQNISTL3, spConnectProtocol);
    ASSERT_TRUE(optRequest);
    upClientMediator->BindPeer(spClientPeer); // Bind the client mediator to the client peer. 
    
    // Setup an exchange through the mediator as the acceptor. 
    EXPECT_TRUE(upServerMediator->SetupExchangeAcceptor(Security::Strategy::PQNISTL3));
    upServerMediator->BindPeer(spServerPeer); // Bind the server mediator to the server peer. 

    EXPECT_TRUE(spClientPeer->ScheduleSend(test::EndpointIdentifier, *optRequest));

    // We expect that the connect protocol has been used once. 
    EXPECT_TRUE(spConnectProtocol->CalledOnce());

    EXPECT_EQ(upClientMediator->GetSecurityState(), Security::State::Authorized);
    EXPECT_TRUE(spConnectProtocol->CalledBy(test::ClientIdentifier));
    EXPECT_EQ(spClientPeer->GetSentCount(), Security::PQNISTL3::CStrategy::AcceptorStages + 1);

    EXPECT_EQ(upServerMediator->GetSecurityState(), Security::State::Authorized);
    EXPECT_FALSE(spConnectProtocol->CalledBy(test::ServerIdentifier));
    EXPECT_EQ(spServerPeer->GetSentCount(), Security::PQNISTL3::CStrategy::InitiatorStages);

    EXPECT_EQ(spCollector->GetCollectedData(), test::ConnectMessage);

    auto const optClientContext = spClientPeer->GetMessageContext(test::EndpointIdentifier);
    ASSERT_TRUE(optClientContext);

    auto const optApplicationRequest = CApplicationMessage::Builder()
        .SetMessageContext(*optClientContext)
        .SetSource(*test::ClientIdentifier)
        .SetDestination(*test::ServerIdentifier)
        .SetCommand(Command::Type::Information, 0)
        .SetPayload("Information Request")
        .ValidatedBuild();
    ASSERT_TRUE(optApplicationRequest);

    std::string const request = optApplicationRequest->GetPack();
    EXPECT_TRUE(spClientPeer->ScheduleSend(test::EndpointIdentifier, request));
    EXPECT_EQ(spCollector->GetCollectedPack(), request);

    auto const optServerContext = spServerPeer->GetMessageContext(test::EndpointIdentifier);
    ASSERT_TRUE(optServerContext);

    auto const optApplicationResponse = CApplicationMessage::Builder()
        .SetMessageContext(*optServerContext)
        .SetSource(*test::ClientIdentifier)
        .SetDestination(*test::ServerIdentifier)
        .SetCommand(Command::Type::Information, 1)
        .SetPayload("Information Response")
        .ValidatedBuild();
    ASSERT_TRUE(optApplicationResponse);

    std::string const response = optApplicationResponse->GetPack();
    EXPECT_TRUE(spServerPeer->ScheduleSend(test::EndpointIdentifier, response));
    EXPECT_EQ(spCollector->GetCollectedPack(), response);
}

//------------------------------------------------------------------------------------------------

local::CConnectProtocolStub::CConnectProtocolStub()
    : m_callers()
{
}

//------------------------------------------------------------------------------------------------

bool local::CConnectProtocolStub::SendRequest(
    BryptIdentifier::SharedContainer const& spSourceIdentifier,
    std::shared_ptr<CBryptPeer> const& spBryptPeer,
    CMessageContext const& context) const
{
    if (!spSourceIdentifier || !spBryptPeer) {
        return false;
    }

    m_callers.emplace_back(spSourceIdentifier->GetInternalRepresentation());

    auto const optConnectRequest = CApplicationMessage::Builder()
        .SetMessageContext(context)
        .SetSource(*test::ClientIdentifier)
        .SetDestination(*test::ServerIdentifier)
        .SetCommand(Command::Type::Connect, 0)
        .SetPayload(test::ConnectMessage)
        .ValidatedBuild();
    assert(optConnectRequest);

    return spBryptPeer->ScheduleSend(
        context.GetEndpointIdentifier(), optConnectRequest->GetPack());
}

//------------------------------------------------------------------------------------------------

bool local::CConnectProtocolStub::CalledBy(
    BryptIdentifier::SharedContainer const& spBryptIdentifier) const
{
    auto const itr = std::find(
        m_callers.begin(), m_callers.end(), spBryptIdentifier->GetInternalRepresentation());

    return (itr != m_callers.end());
}

//------------------------------------------------------------------------------------------------

bool local::CConnectProtocolStub::CalledOnce() const
{
    return m_callers.size();
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

std::uint32_t local::CStrategyStub::GetSignatureSize() const
{
    return 0;
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

std::int32_t local::CStrategyStub::Sign(
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

std::int32_t local::CStrategyStub::Sign(
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
    , m_data()
{
}

//------------------------------------------------------------------------------------------------

bool local::CMessageCollector::CollectMessage(
    [[maybe_unused]] std::weak_ptr<CBryptPeer> const& wpBryptPeer,
    CMessageContext const& context,
    std::string_view buffer)
{
    m_pack = std::string(buffer.begin(), buffer.end());

    auto decoded = PackUtils::Z85Decode(buffer);
    auto const optProtocol = Message::PeekProtocol(decoded.begin(), decoded.end());
    if (!optProtocol) {
        return false;
    }

    switch (*optProtocol) {
        case Message::Protocol::Application: {
            auto const optMessage = CApplicationMessage::Builder()
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

//------------------------------------------------------------------------------------------------

bool local::CMessageCollector::CollectMessage(
    [[maybe_unused]] std::weak_ptr<CBryptPeer> const& wpBryptPeer,
    [[maybe_unused]] CMessageContext const& context,
    [[maybe_unused]] Message::Buffer const& buffer)
{
    return false;
}

//------------------------------------------------------------------------------------------------

std::string local::CMessageCollector::GetCollectedPack() const
{
    return m_pack;
}

//------------------------------------------------------------------------------------------------

std::string local::CMessageCollector::GetCollectedData() const
{
    return m_data;
}

//------------------------------------------------------------------------------------------------
