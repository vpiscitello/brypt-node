//------------------------------------------------------------------------------------------------
#include "../../BryptIdentifier/BryptIdentifier.hpp"
#include "../../BryptMessage/HandshakeMessage.hpp"
#include "../../Components/BryptPeer/BryptPeer.hpp"
#include "../../Components/Endpoints/EndpointIdentifier.hpp"
#include "../../Components/Endpoints/TechnologyType.hpp"
#include "../../Components/MessageControl/ExchangeProcessor.hpp"
#include "../../Components/Security/PostQuantum/NISTSecurityLevelThree.hpp"
#include "../../Components/Security/SecurityUtils.hpp"
#include "../../Interfaces/ExchangeObserver.hpp"
//------------------------------------------------------------------------------------------------
#include "../../Libraries/googletest/include/gtest/gtest.h"
//------------------------------------------------------------------------------------------------
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
namespace {
namespace local {
//------------------------------------------------------------------------------------------------

class CExchangeObserverStub;

//------------------------------------------------------------------------------------------------
} // local namespace
//------------------------------------------------------------------------------------------------
namespace test {
//------------------------------------------------------------------------------------------------

BryptIdentifier::CContainer const ClientIdentifier(BryptIdentifier::Generate());
BryptIdentifier::CContainer const ServerIdentifier(BryptIdentifier::Generate());

constexpr Endpoints::EndpointIdType const EndpointIdentifier = 1;
constexpr Endpoints::TechnologyType const EndpointTechnology = Endpoints::TechnologyType::TCP;
CMessageContext const MessageContext(EndpointIdentifier, EndpointTechnology);

//------------------------------------------------------------------------------------------------
} // local namespace
} // namespace
//------------------------------------------------------------------------------------------------

class local::CExchangeObserverStub : public IExchangeObserver
{
public:
    CExchangeObserverStub();

    // IExchangeObserver {
    virtual void HandleExchangeClose(
        ExchangeStatus status,
        std::unique_ptr<ISecurityStrategy>&& upSecurityStrategy = nullptr) override;
    // } IExchangeObserver 

    bool ExchangeSuccess() const;

private: 
    ExchangeStatus m_status;
    std::unique_ptr<ISecurityStrategy> m_upSecurityStrategy;

};

//------------------------------------------------------------------------------------------------

TEST(ExchangeProcessorSuite, PQNISTL3KeyShareTest)
{
    // Declare the server resoucres for the test. 
    std::shared_ptr<CBryptPeer> spServerPeer;
    std::unique_ptr<ISecurityStrategy> upServerStrategy;
    std::unique_ptr<local::CExchangeObserverStub> upServerObserver;
    std::unique_ptr<CExchangeProcessor> upServerProcessor;

    // Declare the client resources for the test. 
    std::shared_ptr<CBryptPeer> spClientPeer;
    std::unique_ptr<ISecurityStrategy> upClientStrategy;
    std::unique_ptr<local::CExchangeObserverStub> upClientObserver;
    std::unique_ptr<CExchangeProcessor> upClientProcessor;

    // Setup the client side of the exchange. 
    {
        // Make the client peer and register a mock endpoint to handle exchange messages.
        spClientPeer = std::make_shared<CBryptPeer>(test::ClientIdentifier);
        spClientPeer->RegisterEndpoint(
            test::EndpointIdentifier,
            test::EndpointTechnology,
            [&spServerPeer, &upClientProcessor] (
                [[maybe_unused]] auto const& destination, std::string_view message) -> bool
            {
                // Use the client processor to handle the received message. 
                EXPECT_TRUE(
                    upClientProcessor->CollectMessage(spServerPeer, test::MessageContext, message));
                return true;
            });

        // Setup an observer for the client processor.
        upClientObserver = std::make_unique<local::CExchangeObserverStub>();

        // Setup a security strategy for the client processor. 
        upClientStrategy = Security::CreateStrategy(
            Security::Strategy::PQNISTL3, Security::Role::Initiator, Security::Context::Unique);

        // Setup the client processor. 
        upClientProcessor = std::make_unique<CExchangeProcessor>(
            spClientPeer->GetBryptIdentifier(), upClientObserver.get(), std::move(upClientStrategy));
    }

    // Prepare the client processor for the exchange. The processor will tell us if the exchange 
    // could be prepared and the request that needs to sent to the server. 
    auto const [success, request] = upClientProcessor->Prepare();
    EXPECT_TRUE(success);
    EXPECT_GT(request.size(), std::uint32_t(0));

    // Setup the server side of the exchange. 
    {
        // Make the server peer and register a mock endpoint to handle exchange messages.
        spServerPeer = std::make_shared<CBryptPeer>(test::ServerIdentifier);
        spServerPeer->RegisterEndpoint(
            test::EndpointIdentifier,
            test::EndpointTechnology,
            [&spClientPeer, &upServerProcessor] (
                [[maybe_unused]] auto const& destination, std::string_view message) -> bool
            {
                EXPECT_TRUE(
                    upServerProcessor->CollectMessage(spClientPeer, test::MessageContext, message));
                return true;
            });

        // Setup an observer for the server processor.
        upServerObserver = std::make_unique<local::CExchangeObserverStub>();

        // Setup a security strategy for the server processor. 
        upServerStrategy = Security::CreateStrategy(
            Security::Strategy::PQNISTL3, Security::Role::Acceptor, Security::Context::Unique);

        // Setup the server processor. 
        upServerProcessor = std::make_unique<CExchangeProcessor>(
            spServerPeer->GetBryptIdentifier(), upServerObserver.get(), std::move(upServerStrategy));

        // Prepare the server processor for the exchange. The tell us if the preparation succeeded. 
        // We do not expect to given an initial message to be sent given it is the acceptor. 
        auto const [success, buffer] = upServerProcessor->Prepare();
        EXPECT_TRUE(success);
        EXPECT_EQ(buffer.size(), std::uint32_t(0));

        // Start of the exchange by manually giving the server processor the client processor's 
        // initial request. This will cause the exchange transaction to occur of the stack rather
        // than over a network. 
        EXPECT_TRUE(
            upServerProcessor->CollectMessage(spClientPeer, test::MessageContext, request));
    }

    // After the exchange occurs on the stack we expect that both the client and server processors
    // have successfully completed the exchange. 
    EXPECT_TRUE(upClientObserver->ExchangeSuccess());
    EXPECT_TRUE(upServerObserver->ExchangeSuccess());
}

//------------------------------------------------------------------------------------------------

local::CExchangeObserverStub::CExchangeObserverStub()
    : m_status(ExchangeStatus::Failed)
    , m_upSecurityStrategy()
{
}

//------------------------------------------------------------------------------------------------

void local::CExchangeObserverStub::HandleExchangeClose(
    ExchangeStatus status,
    std::unique_ptr<ISecurityStrategy>&& upSecurityStrategy)
{
    m_status = status;
    m_upSecurityStrategy = std::move(upSecurityStrategy);
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
