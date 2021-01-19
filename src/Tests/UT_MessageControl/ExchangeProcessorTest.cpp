//------------------------------------------------------------------------------------------------
#include "BryptIdentifier/BryptIdentifier.hpp"
#include "BryptMessage/NetworkMessage.hpp"
#include "Components/BryptPeer/BryptPeer.hpp"
#include "Components/MessageControl/ExchangeProcessor.hpp"
#include "Components/Network/EndpointIdentifier.hpp"
#include "Components/Network/Protocol.hpp"
#include "Components/Security/SecurityUtils.hpp"
#include "Components/Security/PostQuantum/NISTSecurityLevelThree.hpp"
#include "Interfaces/ConnectProtocol.hpp"
#include "Interfaces/ExchangeObserver.hpp"
//------------------------------------------------------------------------------------------------
#include <gtest/gtest.h>
//------------------------------------------------------------------------------------------------
#include <algorithm>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>
//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
namespace {
namespace local {
//------------------------------------------------------------------------------------------------

class CConnectProtocolStub;
class CExchangeObserverStub;

//------------------------------------------------------------------------------------------------
} // local namespace
//------------------------------------------------------------------------------------------------
namespace test {
//------------------------------------------------------------------------------------------------

auto const ClientIdentifier = std::make_shared<BryptIdentifier::CContainer>(
    BryptIdentifier::Generate());
auto const ServerIdentifier = std::make_shared<BryptIdentifier::CContainer>(
    BryptIdentifier::Generate());

constexpr Endpoints::EndpointIdType const EndpointIdentifier = 1;
constexpr Endpoints::TechnologyType const EndpointTechnology = Endpoints::TechnologyType::TCP;

constexpr std::string_view ExchangeCloseMessage = "Exchange Success!";

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

//------------------------------------------------------------------------------------------------

TEST(ExchangeProcessorSuite, PQNISTL3KeyShareTest)
{
    // Declare the client resources for the test. 
    std::shared_ptr<CBryptPeer> spClientPeer;
    std::unique_ptr<ISecurityStrategy> upClientStrategy;
    std::unique_ptr<local::CExchangeObserverStub> upClientObserver;
    std::unique_ptr<CExchangeProcessor> upClientProcessor;

    // Declare the server resoucres for the test. 
    std::shared_ptr<CBryptPeer> spServerPeer;
    std::unique_ptr<ISecurityStrategy> upServerStrategy;
    std::unique_ptr<local::CExchangeObserverStub> upServerObserver;
    std::unique_ptr<CExchangeProcessor> upServerProcessor;

    auto const spConnectProtocol = std::make_shared<local::CConnectProtocolStub>();

    // Setup the client's view of the exchange.
    {
        // Make the client peer and register a mock endpoint to handle exchange messages.
        spClientPeer = std::make_shared<CBryptPeer>(*test::ServerIdentifier);
        spClientPeer->RegisterEndpoint(
            test::EndpointIdentifier,
            test::EndpointTechnology,
            [&spServerPeer] (
                [[maybe_unused]] auto const& destination, std::string_view message) -> bool
            {
                return spServerPeer->ScheduleReceive(test::EndpointIdentifier, message);
            });

        // Setup an observer for the client processor.
        upClientObserver = std::make_unique<local::CExchangeObserverStub>();

        // Setup a security strategy for the client processor. 
        upClientStrategy = Security::CreateStrategy(
            Security::Strategy::PQNISTL3, Security::Role::Initiator, Security::Context::Unique);

        // Setup the client processor. 
        upClientProcessor = std::make_unique<CExchangeProcessor>(
            test::ClientIdentifier,
            spConnectProtocol,
            upClientObserver.get(),
            std::move(upClientStrategy));

        spClientPeer->SetReceiver(upClientProcessor.get());
    }

    // Setup the server's view of the exchange.
    {
        // Make the server peer and register a mock endpoint to handle exchange messages.
        spServerPeer = std::make_shared<CBryptPeer>(*test::ClientIdentifier);
        spServerPeer->RegisterEndpoint(
            test::EndpointIdentifier,
            test::EndpointTechnology,
            [&spClientPeer] (
                [[maybe_unused]] auto const& destination, std::string_view message) -> bool
            {
                return spClientPeer->ScheduleReceive(test::EndpointIdentifier, message);
            });

        // Setup an observer for the server processor.
        upServerObserver = std::make_unique<local::CExchangeObserverStub>();

        // Setup a security strategy for the server processor. 
        upServerStrategy = Security::CreateStrategy(
            Security::Strategy::PQNISTL3, Security::Role::Acceptor, Security::Context::Unique);

        // Setup the server processor. 
        upServerProcessor = std::make_unique<CExchangeProcessor>(
            test::ServerIdentifier, nullptr, upServerObserver.get(), std::move(upServerStrategy));

        spServerPeer->SetReceiver(upServerProcessor.get());
    }

    // Prepare the client processor for the exchange. The processor will tell us if the exchange 
    // could be prepared and the request that needs to sent to the server. 
    auto const clientPrepareResult = upClientProcessor->Prepare();
    EXPECT_TRUE(clientPrepareResult.first);
    EXPECT_GT(clientPrepareResult.second.size(), std::uint32_t(0));

    // Prepare the server processor for the exchange. The processor will tell us if the preparation
    // succeeded. We do not expect to given an initial message to be sent given it is the acceptor. 
    auto const serverPrepareResult = upServerProcessor->Prepare();
    EXPECT_TRUE(serverPrepareResult.first);
    EXPECT_EQ(serverPrepareResult.second.size(), std::uint32_t(0));

    // Start of the exchange by manually telling the client peer to send the exchange request.
    // This will cause the exchange transaction to occur of the stack. 
    EXPECT_TRUE(spClientPeer->ScheduleSend(test::EndpointIdentifier, clientPrepareResult.second));

    // We expect that the connect protocol has been used once. 
    EXPECT_TRUE(spConnectProtocol->CalledOnce());

    // We expect that the client observer was notified of a successful exchange, the connect 
    // protocol was called by client exchange, and the client peer sent the number of messages
    // required by the server. 
    EXPECT_TRUE(upClientObserver->ExchangeSuccess());
    EXPECT_TRUE(spConnectProtocol->CalledBy(test::ClientIdentifier));
    EXPECT_EQ(spClientPeer->GetSentCount(), Security::PQNISTL3::CStrategy::AcceptorStages);

    // We expect that the server observer was notified of a successful exchange, the connect 
    // protocol was called by server exchange, and the server peer sent the number of messages
    // required by the client. 
    EXPECT_TRUE(upServerObserver->ExchangeSuccess());
    EXPECT_FALSE(spConnectProtocol->CalledBy(test::ServerIdentifier));
    EXPECT_EQ(spServerPeer->GetSentCount(), Security::PQNISTL3::CStrategy::InitiatorStages);
}

//------------------------------------------------------------------------------------------------

local::CConnectProtocolStub::CConnectProtocolStub()
    : m_callers(0)
{
}

//------------------------------------------------------------------------------------------------

bool local::CConnectProtocolStub::SendRequest(
    BryptIdentifier::SharedContainer const& spSourceIdentifier,
    [[maybe_unused]] std::shared_ptr<CBryptPeer> const& spBryptPeer,
    [[maybe_unused]] CMessageContext const& context) const
{
    m_callers.emplace_back(spSourceIdentifier->GetInternalRepresentation());
    return true;
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

local::CExchangeObserverStub::CExchangeObserverStub()
    : m_status(ExchangeStatus::Failed)
    , m_upSecurityStrategy()
{
}

//------------------------------------------------------------------------------------------------

void local::CExchangeObserverStub::OnExchangeClose(ExchangeStatus status)
{
    m_status = status;
}

//------------------------------------------------------------------------------------------------

void local::CExchangeObserverStub::OnFulfilledStrategy(
    std::unique_ptr<ISecurityStrategy>&& upStrategy)
{
    m_upSecurityStrategy = std::move(upStrategy);
}

//------------------------------------------------------------------------------------------------

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

//------------------------------------------------------------------------------------------------
