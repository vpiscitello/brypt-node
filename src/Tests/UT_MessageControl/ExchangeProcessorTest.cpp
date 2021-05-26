//----------------------------------------------------------------------------------------------------------------------
#include "BryptIdentifier/BryptIdentifier.hpp"
#include "BryptMessage/NetworkMessage.hpp"
#include "Components/MessageControl/ExchangeProcessor.hpp"
#include "Components/Network/EndpointIdentifier.hpp"
#include "Components/Network/Protocol.hpp"
#include "Components/Peer/Proxy.hpp"
#include "Components/Security/SecurityUtils.hpp"
#include "Components/Security/PostQuantum/NISTSecurityLevelThree.hpp"
#include "Interfaces/ConnectProtocol.hpp"
#include "Interfaces/ExchangeObserver.hpp"
#include "Utilities/InvokeContext.hpp"
//----------------------------------------------------------------------------------------------------------------------
#include <gtest/gtest.h>
//----------------------------------------------------------------------------------------------------------------------
#include <algorithm>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>
//----------------------------------------------------------------------------------------------------------------------

//----------------------------------------------------------------------------------------------------------------------
namespace {
namespace local {
//----------------------------------------------------------------------------------------------------------------------

class ConnectProtocolStub;
class CExchangeObserverStub;

//----------------------------------------------------------------------------------------------------------------------
} // local namespace
//----------------------------------------------------------------------------------------------------------------------
namespace test {
//----------------------------------------------------------------------------------------------------------------------

auto const ClientIdentifier = std::make_shared<Node::Identifier>(Node::GenerateIdentifier());
auto const ServerIdentifier = std::make_shared<Node::Identifier>(Node::GenerateIdentifier());

constexpr Network::Endpoint::Identifier const EndpointIdentifier = 1;
constexpr Network::Protocol const EndpointProtocol = Network::Protocol::TCP;

constexpr std::string_view ExchangeCloseMessage = "Exchange Success!";

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

class local::CExchangeObserverStub : public IExchangeObserver
{
public:
    CExchangeObserverStub();

    // IExchangeObserver {
    virtual void OnExchangeClose(ExchangeStatus status) override;
    virtual void OnFulfilledStrategy(std::unique_ptr<ISecurityStrategy>&& upStrategy) override;
    // } IExchangeObserver 

    bool ExchangeSuccess() const;

private: 
    ExchangeStatus m_status;
    std::unique_ptr<ISecurityStrategy> m_upSecurityStrategy;

};

//----------------------------------------------------------------------------------------------------------------------

TEST(ExchangeProcessorSuite, PQNISTL3KeyShareTest)
{
    // Declare the client resources for the test. 
    std::shared_ptr<Peer::Proxy> spClientPeer;
    std::unique_ptr<ISecurityStrategy> upClientStrategy;
    std::unique_ptr<local::CExchangeObserverStub> upClientObserver;
    std::unique_ptr<ExchangeProcessor> upClientProcessor;

    // Declare the server resoucres for the test. 
    std::shared_ptr<Peer::Proxy> spServerPeer;
    std::unique_ptr<ISecurityStrategy> upServerStrategy;
    std::unique_ptr<local::CExchangeObserverStub> upServerObserver;
    std::unique_ptr<ExchangeProcessor> upServerProcessor;

    auto const spConnectProtocol = std::make_shared<local::ConnectProtocolStub>();

    // Setup the client's view of the exchange.
    {
        // Make the client peer and register a mock endpoint to handle exchange messages.
        spClientPeer = std::make_shared<Peer::Proxy>(*test::ServerIdentifier);
        spClientPeer->RegisterSilentEndpoint<InvokeContext::Test>(
            test::EndpointIdentifier,
            test::EndpointProtocol,
            test::RemoteServerAddress,
            [&spServerPeer] ([[maybe_unused]] auto const& destination, auto&& message) -> bool
            {
                return spServerPeer->ScheduleReceive(test::EndpointIdentifier, std::get<std::string>(message));
            });

        // Setup an observer for the client processor.
        upClientObserver = std::make_unique<local::CExchangeObserverStub>();

        // Setup a security strategy for the client processor. 
        upClientStrategy = Security::CreateStrategy(
            Security::Strategy::PQNISTL3, Security::Role::Initiator, Security::Context::Unique);

        // Setup the client processor. 
        upClientProcessor = std::make_unique<ExchangeProcessor>(
            test::ClientIdentifier,
            spConnectProtocol,
            upClientObserver.get(),
            std::move(upClientStrategy));

        spClientPeer->SetReceiver(upClientProcessor.get());
    }

    // Setup the server's view of the exchange.
    {
        // Make the server peer and register a mock endpoint to handle exchange messages.
        spServerPeer = std::make_shared<Peer::Proxy>(*test::ClientIdentifier);
        spServerPeer->RegisterSilentEndpoint<InvokeContext::Test>(
            test::EndpointIdentifier,
            test::EndpointProtocol,
            test::RemoteClientAddress,
            [&spClientPeer] ([[maybe_unused]] auto const& destination, auto&& message) -> bool
            {
                return spClientPeer->ScheduleReceive(test::EndpointIdentifier, std::get<std::string>(message));
            });

        // Setup an observer for the server processor.
        upServerObserver = std::make_unique<local::CExchangeObserverStub>();

        // Setup a security strategy for the server processor. 
        upServerStrategy = Security::CreateStrategy(
            Security::Strategy::PQNISTL3, Security::Role::Acceptor, Security::Context::Unique);

        // Setup the server processor. 
        upServerProcessor = std::make_unique<ExchangeProcessor>(
            test::ServerIdentifier, nullptr, upServerObserver.get(), std::move(upServerStrategy));

        spServerPeer->SetReceiver(upServerProcessor.get());
    }

    // Prepare the client processor for the exchange. The processor will tell us if the exchange 
    // could be prepared and the request that needs to sent to the server. 
    auto clientPrepareResult = upClientProcessor->Prepare();
    EXPECT_TRUE(clientPrepareResult.first);
    EXPECT_GT(clientPrepareResult.second.size(), std::uint32_t(0));

    // Prepare the server processor for the exchange. The processor will tell us if the preparation
    // succeeded. We do not expect to given an initial message to be sent given it is the acceptor. 
    auto const serverPrepareResult = upServerProcessor->Prepare();
    EXPECT_TRUE(serverPrepareResult.first);
    EXPECT_EQ(serverPrepareResult.second.size(), std::uint32_t(0));

    // Start of the exchange by manually telling the client peer to send the exchange request.
    // This will cause the exchange transaction to occur of the stack. 
    EXPECT_TRUE(spClientPeer->ScheduleSend(test::EndpointIdentifier, std::move(clientPrepareResult.second)));

    // We expect that the connect protocol has been used once. 
    EXPECT_TRUE(spConnectProtocol->CalledOnce());

    // We expect that the client observer was notified of a successful exchange, the connect 
    // protocol was called by client exchange, and the client peer sent the number of messages
    // required by the server. 
    EXPECT_TRUE(upClientObserver->ExchangeSuccess());
    EXPECT_TRUE(spConnectProtocol->CalledBy(test::ClientIdentifier));
    EXPECT_EQ(spClientPeer->GetSentCount(), Security::PQNISTL3::Strategy::AcceptorStages);

    // We expect that the server observer was notified of a successful exchange, the connect 
    // protocol was called by server exchange, and the server peer sent the number of messages
    // required by the client. 
    EXPECT_TRUE(upServerObserver->ExchangeSuccess());
    EXPECT_FALSE(spConnectProtocol->CalledBy(test::ServerIdentifier));
    EXPECT_EQ(spServerPeer->GetSentCount(), Security::PQNISTL3::Strategy::InitiatorStages);
}

//----------------------------------------------------------------------------------------------------------------------

local::ConnectProtocolStub::ConnectProtocolStub()
    : m_callers(0)
{
}

//----------------------------------------------------------------------------------------------------------------------

bool local::ConnectProtocolStub::SendRequest(
    Node::SharedIdentifier const& spSourceIdentifier,
    [[maybe_unused]] std::shared_ptr<Peer::Proxy> const& spPeerProxy,
    [[maybe_unused]] MessageContext const& context) const
{
    m_callers.emplace_back(spSourceIdentifier->GetInternalValue());
    return true;
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

local::CExchangeObserverStub::CExchangeObserverStub()
    : m_status(ExchangeStatus::Failed)
    , m_upSecurityStrategy()
{
}

//----------------------------------------------------------------------------------------------------------------------

void local::CExchangeObserverStub::OnExchangeClose(ExchangeStatus status)
{
    m_status = status;
}

//----------------------------------------------------------------------------------------------------------------------

void local::CExchangeObserverStub::OnFulfilledStrategy(
    std::unique_ptr<ISecurityStrategy>&& upStrategy)
{
    m_upSecurityStrategy = std::move(upStrategy);
}

//----------------------------------------------------------------------------------------------------------------------

bool local::CExchangeObserverStub::ExchangeSuccess() const
{
    // The exchange was successful if we were notified of a success and acquired a synchronized
    // security strategy. 
    bool const success = (m_status == ExchangeStatus::Success);
    bool const ready = (
        m_upSecurityStrategy &&
        m_upSecurityStrategy->GetSynchronizationStatus() == Security::SynchronizationStatus::Ready);
    return (success && ready);
}

//----------------------------------------------------------------------------------------------------------------------
