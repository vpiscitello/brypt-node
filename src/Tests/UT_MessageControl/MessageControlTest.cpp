//------------------------------------------------------------------------------------------------
#include "../../BryptIdentifier/BryptIdentifier.hpp"
#include "../../Components/Endpoints/Endpoint.hpp"
#include "../../Components/Endpoints/DirectEndpoint.hpp"
#include "../../Components/Endpoints/TechnologyType.hpp"
#include "../../Components/MessageControl/AssociatedMessage.hpp"
#include "../../Components/MessageControl/MessageCollector.hpp"
#include "../../BryptMessage/ApplicationMessage.hpp"
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

//------------------------------------------------------------------------------------------------
} // local namespace
//------------------------------------------------------------------------------------------------
namespace test {
//------------------------------------------------------------------------------------------------

BryptIdentifier::CContainer const ClientIdentifier(BryptIdentifier::Generate());
auto const spServerIdentifier = std::make_shared<BryptIdentifier::CContainer const>(
    BryptIdentifier::Generate());

constexpr Command::Type Command = Command::Type::Election;
constexpr std::uint8_t RequestPhase = 0;
constexpr std::uint8_t ResponsePhase = 1;
constexpr std::string_view Message = "Hello World!";
constexpr std::uint32_t Nonce = 0;

constexpr Endpoints::EndpointIdType const EndpointIdentifier = 1;
constexpr Endpoints::TechnologyType const EndpointTechnology = Endpoints::TechnologyType::TCP;
CMessageContext const MessageContext(EndpointIdentifier, EndpointTechnology);

constexpr std::uint32_t Iterations = 10000;

//------------------------------------------------------------------------------------------------
} // local namespace
} // namespace
//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------

TEST(CMessageCollectorSuite, SingleMessageCollectionTest)
{
    CMessageCollector collector;

    std::optional<CApplicationMessage> optForwardedResponse = {};
    auto const spClientPeer = std::make_shared<CBryptPeer>(test::ClientIdentifier);
    spClientPeer->RegisterEndpoint(
        test::EndpointIdentifier,
        test::EndpointTechnology,
        [&optForwardedResponse] (CApplicationMessage const& message) -> bool
        {
            Message::ValidationStatus status = message.Validate();
            if (status != Message::ValidationStatus::Success) {
                return false;
            }
            optForwardedResponse = message;
            return true;
        });

    auto const optRequest = CApplicationMessage::Builder()
        .SetMessageContext(test::MessageContext)
        .SetSource(test::ClientIdentifier)
        .SetDestination(*test::spServerIdentifier)
        .SetCommand(test::Command, test::RequestPhase)
        .SetData(test::Message)
        .ValidatedBuild();

    collector.CollectMessage(spClientPeer, *optRequest);

    EXPECT_EQ(collector.QueuedMessageCount(), std::uint32_t(1));
    
    auto const optAssociatedMessage = collector.PopIncomingMessage();
    ASSERT_TRUE(optAssociatedMessage);
    EXPECT_EQ(collector.QueuedMessageCount(), std::uint32_t(0));

    auto& [wpClientRequestPeer, request] = *optAssociatedMessage;
    EXPECT_EQ(optRequest->GetPack(), request.GetPack());

    auto const optResponse = CApplicationMessage::Builder()
        .SetMessageContext(test::MessageContext)
        .SetSource(*test::spServerIdentifier)
        .SetDestination(test::ClientIdentifier)
        .SetCommand(test::Command, test::ResponsePhase)
        .SetData(test::Message)
        .ValidatedBuild();

    if (auto const spClientRequestPeer = wpClientRequestPeer.lock(); spClientRequestPeer) {
        EXPECT_EQ(spClientRequestPeer, spClientPeer);
        spClientRequestPeer->ScheduleSend(*optResponse);
    } else {
        ASSERT_FALSE(true);
    }

    EXPECT_EQ(optForwardedResponse->GetPack(), optResponse->GetPack());
}

//------------------------------------------------------------------------------------------------

TEST(CMessageCollectorSuite, MultipleMessageCollectionTest)
{
    CMessageCollector collector;

    std::optional<CApplicationMessage> optForwardedResponse = {};
    auto const spClientPeer = std::make_shared<CBryptPeer>(test::ClientIdentifier);
    spClientPeer->RegisterEndpoint(
        test::EndpointIdentifier,
        test::EndpointTechnology,
        [&optForwardedResponse] (CApplicationMessage const& message) -> bool
        {
            Message::ValidationStatus status = message.Validate();
            if (status != Message::ValidationStatus::Success) {
                return false;
            }
            optForwardedResponse = message;
            return true;
        });

    for (std::uint32_t count = 0; count < test::Iterations; ++count) {
        auto const optRequest = CApplicationMessage::Builder()
            .SetMessageContext(test::MessageContext)
            .SetSource(test::ClientIdentifier)
            .SetDestination(*test::spServerIdentifier)
            .SetCommand(test::Command, test::RequestPhase)
            .SetData(test::Message)
            .ValidatedBuild();

        collector.CollectMessage(spClientPeer, *optRequest);
    }

    EXPECT_EQ(collector.QueuedMessageCount(), std::uint32_t(test::Iterations));
    
    for (std::uint32_t count = test::Iterations; count > 0; --count) {
        auto const optAssociatedMessage = collector.PopIncomingMessage();
        ASSERT_TRUE(optAssociatedMessage);
        EXPECT_EQ(collector.QueuedMessageCount(), std::uint32_t(count - 1));

        auto& [wpClientRequestPeer, request] = *optAssociatedMessage;

        auto const optResponse = CApplicationMessage::Builder()
            .SetMessageContext(test::MessageContext)
            .SetSource(*test::spServerIdentifier)
            .SetDestination(test::ClientIdentifier)
            .SetCommand(test::Command, test::ResponsePhase)
            .SetData(test::Message)
            .ValidatedBuild();

        if (auto const spClientRequestPeer = wpClientRequestPeer.lock(); spClientRequestPeer) {
            EXPECT_EQ(spClientRequestPeer, spClientPeer);
            spClientRequestPeer->ScheduleSend(*optResponse);
        } else {
            ASSERT_FALSE(true);
        }

        EXPECT_EQ(optForwardedResponse->GetPack(), optResponse->GetPack());
    }

    EXPECT_EQ(collector.QueuedMessageCount(), std::uint32_t(0));
}

//------------------------------------------------------------------------------------------------
