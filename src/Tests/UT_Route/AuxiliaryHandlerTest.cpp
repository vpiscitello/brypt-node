//----------------------------------------------------------------------------------------------------------------------
#include "TestHelpers.hpp"
#include "Components/Core/ServiceProvider.hpp"
#include "Components/Message/ApplicationMessage.hpp"
#include "Components/Peer/Action.hpp"
#include "Components/Peer/Proxy.hpp"
#include "Components/Route/Auxiliary.hpp"
#include "Components/Route/MessageHandler.hpp"
#include "Components/Route/Router.hpp"
#include "Components/Scheduler/Registrar.hpp"
#include "Components/State/NodeState.hpp"
//----------------------------------------------------------------------------------------------------------------------
#include <gtest/gtest.h>
//----------------------------------------------------------------------------------------------------------------------
#include <cstdint>
#include <chrono>
#include <set>
#include <string>
#include <string_view>
#include <vector>
//----------------------------------------------------------------------------------------------------------------------

//----------------------------------------------------------------------------------------------------------------------
namespace {
namespace local {
//----------------------------------------------------------------------------------------------------------------------

//----------------------------------------------------------------------------------------------------------------------
} // local namespace
//----------------------------------------------------------------------------------------------------------------------
namespace test {
//----------------------------------------------------------------------------------------------------------------------

auto const ClientIdentifier = Node::Identifier{ Node::GenerateIdentifier() };
auto const ServerIdentifier = std::make_shared<Node::Identifier>(Node::GenerateIdentifier());

constexpr std::string_view AuxiliaryRoute = "/auxiliary";
constexpr std::string_view const RequestPayload = "Request Payload";
constexpr std::string_view const ResponsePayload = "Response Payload";

//----------------------------------------------------------------------------------------------------------------------
} // local namespace
} // namespace
//----------------------------------------------------------------------------------------------------------------------

TEST(AuxillaryHandlerSuite, ExternalHandlerTest)
{
    auto const spRegistrar = std::make_shared<Scheduler::Registrar>();
    auto const spServiceProvider = std::make_shared<Node::ServiceProvider>();
    auto const spNodeState = std::make_shared<NodeState>(test::ServerIdentifier, Network::ProtocolSet{});
    spServiceProvider->Register(spNodeState);

    Route::Router router;
    std::optional<Message::Application::Parcel> optResponse;

    auto const spProxy = Peer::Proxy::CreateInstance(test::ClientIdentifier, spServiceProvider);
    spProxy->RegisterSilentEndpoint<InvokeContext::Test>(
        Route::Test::EndpointIdentifier, Route::Test::EndpointProtocol, Route::Test::RemoteClientAddress,
        [&spProxy, &optResponse] ([[maybe_unused]] auto const& destination, auto&& message) -> bool {
            auto const optContext = spProxy->GetMessageContext(Route::Test::EndpointIdentifier);
            if (!optContext) { return false; }

            auto optMessage = Message::Application::Parcel::GetBuilder()
                .SetContext(*optContext)
                .FromEncodedPack(std::get<std::string>(message))
                .ValidatedBuild();
            if(!optMessage) { return false; }

            Message::ValidationStatus status = optMessage->Validate();
            if (status != Message::ValidationStatus::Success) { return false; }
            optResponse = std::move(optMessage);
            return true;
        });

    auto const onMessage = [] (Message::Application::Parcel const&, Peer::Action::Next& next) -> bool {
        return next.Respond(test::ResponsePayload, Message::Extension::Status::Ok);
    };

    EXPECT_TRUE(router.Register<Route::Auxiliary::ExternalHandler>(test::AuxiliaryRoute, onMessage));
    EXPECT_TRUE(router.Initialize(spServiceProvider));
    
    auto const optContext = spProxy->GetMessageContext(Route::Test::EndpointIdentifier);
    ASSERT_TRUE(optContext);

    auto const optRequest = Message::Application::Parcel::GetBuilder()
        .SetContext(*optContext)
        .SetSource(test::ClientIdentifier)
        .SetDestination(*test::ServerIdentifier)
        .SetRoute(test::AuxiliaryRoute)
        .SetPayload(test::RequestPayload)
        .BindExtension<Message::Extension::Awaitable>(
            Message::Extension::Awaitable::Request, Route::Test::TrackerKey)
        .ValidatedBuild();
    ASSERT_TRUE(optRequest);

    Peer::Action::Next next{ spProxy, *optRequest, spServiceProvider };
    EXPECT_TRUE(router.Route(*optRequest, next));

    ASSERT_TRUE(optResponse);
    EXPECT_EQ(optResponse->GetSource(), *test::ServerIdentifier);
    EXPECT_EQ(optResponse->GetDestination(), test::ClientIdentifier);
    EXPECT_EQ(optResponse->GetRoute(), test::AuxiliaryRoute);
    EXPECT_EQ(optResponse->GetPayload().GetStringView(), test::ResponsePayload);

    auto const optRequestExtension = optResponse->GetExtension<Message::Extension::Awaitable>();
    EXPECT_TRUE(optRequestExtension);
    EXPECT_EQ(optRequestExtension->get().GetBinding(), Message::Extension::Awaitable::Binding::Response);
    ASSERT_EQ(optRequestExtension->get().GetTracker(), Route::Test::TrackerKey);
}

//----------------------------------------------------------------------------------------------------------------------
