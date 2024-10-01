//----------------------------------------------------------------------------------------------------------------------
#include "TestHelpers.hpp"
#include "Components/Core/ServiceProvider.hpp"
#include "Components/Message/ApplicationMessage.hpp"
#include "Components/Peer/Action.hpp"
#include "Components/Peer/Proxy.hpp"
#include "Components/Route/MessageHandler.hpp"
#include "Components/Route/Router.hpp"
#include "Components/Scheduler/Registrar.hpp"
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

constexpr std::string_view InspectableRoute = "/test/expected/handler";
constexpr std::string_view FailingRoute = "/test/failing/handler";

using RegisterExpectations = std::vector<std::tuple<std::string, bool>>;

class StandardHandler;
class FailingHandler;

class InspectableService;
class InspectableHandler;

//----------------------------------------------------------------------------------------------------------------------
} // local namespace
} // namespace
//----------------------------------------------------------------------------------------------------------------------

class test::StandardHandler : public Route::IMessageHandler
{
public:
    StandardHandler() = default;

    // IMessageHandler{
    [[nodiscard]] virtual bool OnFetchServices(std::shared_ptr<Node::ServiceProvider> const&) override { return true; }
    [[nodiscard]] virtual bool OnMessage(Message::Application::Parcel const&, Peer::Action::Next&) override { return true; }
    // }IMessageHandler
};

//----------------------------------------------------------------------------------------------------------------------

class test::FailingHandler : public Route::IMessageHandler
{
public:
    FailingHandler() = default;

    // IMessageHandler{
    [[nodiscard]] virtual bool OnFetchServices(std::shared_ptr<Node::ServiceProvider> const&) override { return false; }
    [[nodiscard]] virtual bool OnMessage(Message::Application::Parcel const&, Peer::Action::Next&) override { return false; }
    // }IMessageHandler
};

//----------------------------------------------------------------------------------------------------------------------

class test::InspectableService
{
public:
    InspectableService() : m_fetched(0), m_handled(0) {}

    void OnFetched() { ++m_fetched; }
    void OnHandled() { ++m_handled; }

    [[nodiscard]] std::size_t const& GetFetched() const { return m_fetched; }
    [[nodiscard]] std::size_t const& GetHandled() const { return m_handled; }

private:
    std::size_t m_fetched;
    std::size_t m_handled;
};

//----------------------------------------------------------------------------------------------------------------------

class test::InspectableHandler : public Route::IMessageHandler
{
public:
    InspectableHandler() = default;

    // IMessageHandler{
    [[nodiscard]] virtual bool OnFetchServices(std::shared_ptr<Node::ServiceProvider> const&) override;
    [[nodiscard]] virtual bool OnMessage(Message::Application::Parcel const&, Peer::Action::Next&) override;
    // }IMessageHandler

private:
    std::weak_ptr<test::InspectableService> m_wpInspectableService;
};

//----------------------------------------------------------------------------------------------------------------------

class RouterSuite : public testing::Test
{
protected:
    void SetUp() override
    {
        m_spRegistrar = std::make_shared<Scheduler::Registrar>();
        m_spServiceProvider = std::make_shared<Node::ServiceProvider>();

        m_spProxy = Peer::Proxy::CreateInstance(test::ClientIdentifier, m_spServiceProvider);
        m_spProxy->RegisterSilentEndpoint<InvokeContext::Test>(
            Route::Test::EndpointIdentifier, Route::Test::EndpointProtocol, Route::Test::RemoteClientAddress);

        auto const optContext = m_spProxy->GetMessageContext(Route::Test::EndpointIdentifier);
        ASSERT_TRUE(optContext);
        m_context = *optContext;

        m_spInspectableService = std::make_shared<test::InspectableService>();
        m_spServiceProvider->Register(m_spInspectableService);
        EXPECT_EQ(m_spInspectableService->GetFetched(), std::size_t{ 0 });
        EXPECT_EQ(m_spInspectableService->GetHandled(), std::size_t{ 0 });

        test::RegisterExpectations const expectations = {
            { "/query/data", true },
            { "/query/data/history", true },
            { "/query/temperature", true },
            { "/query/humidity", true },
            { "/information/peers/neighbors", true },
            { "/information/power", true },
            { "/repetition/repetition/repetition/repetition", true },
            { "/connect", true },
            { "/query/data", true }, // Currently, replacements are allowed. 
            { "/1", true },
            { "/1/2/3/", true },
            { "/1/2/3/4", true },
            { "", false },
            { " ", false },
            { "/", false },
            { "///", false },
            { "/.", false },
            { "\\query\\data", false },
            { "/query/*", false },
            { "/query/:", false },
            { "/query//", false },
            { "/query?", false },
            { "/query/data//", false },
            { "/query/_/data", false },
            { "/query//data", false },
            { "\"/query\"", false },
        };

        for (auto const& [name, valid] : expectations) {
            // You should be able to register a handler given a valid path. 
            EXPECT_EQ(m_router.Register<test::StandardHandler>(name), valid);

            // You should be able to verify the route's existance directly after registration. 
            EXPECT_EQ(m_router.Contains(name), valid);
        }

        for (auto const& [name, valid] : expectations) {
            // You should be able to verify all valid routes exist after the prefix tree has been adjusted by 
            // other registered routes. 
            EXPECT_EQ(m_router.Contains(name), valid);
        }

        // Register an easily accesible test route.
        EXPECT_TRUE(m_router.Register<test::InspectableHandler>(test::InspectableRoute));
        EXPECT_EQ(m_spInspectableService->GetFetched(), std::size_t{ 0 });

        // By default, the test router should succesfully initialize. 
        EXPECT_TRUE(m_router.Initialize(m_spServiceProvider));
        EXPECT_EQ(m_spInspectableService->GetFetched(), std::size_t{ 1 });
    }


    std::shared_ptr<Scheduler::Registrar> m_spRegistrar;
    std::shared_ptr<Node::ServiceProvider> m_spServiceProvider;
    std::shared_ptr<Peer::Proxy> m_spProxy;
    Message::Context m_context;
    std::shared_ptr<test::InspectableService> m_spInspectableService;
    Route::Router m_router;
};

//----------------------------------------------------------------------------------------------------------------------

TEST_F(RouterSuite, InspectHandlerTest)
{
    EXPECT_EQ(m_spInspectableService->GetHandled(), std::size_t{ 0 });

    auto const optMessage = Message::Application::Parcel::GetBuilder()
        .SetContext(m_context)
        .SetSource(test::ClientIdentifier)
        .SetDestination(*test::ServerIdentifier)
        .SetRoute(test::InspectableRoute)
        .SetPayload(Route::Test::Message)
        .ValidatedBuild();
    ASSERT_TRUE(optMessage);

    Peer::Action::Next next{ m_spProxy, *optMessage, m_spServiceProvider };

    // The router should propogate the handler's failure to handle a message. 
    EXPECT_TRUE(m_router.Route(*optMessage, next));

    EXPECT_EQ(m_spInspectableService->GetHandled(), std::size_t{ 1 });
}

//----------------------------------------------------------------------------------------------------------------------

TEST_F(RouterSuite, FailedHandlerTest)
{
    // The handler fails to fetch services and handle messages, but it's path is still valid. 
    EXPECT_TRUE(m_router.Register<test::FailingHandler>(test::FailingRoute));

    // The router should now indicate it fails to initialize. 
    EXPECT_FALSE(m_router.Initialize(m_spServiceProvider));

    auto const optMessage = Message::Application::Parcel::GetBuilder()
        .SetContext(m_context)
        .SetSource(test::ClientIdentifier)
        .SetDestination(*test::ServerIdentifier)
        .SetRoute(test::FailingRoute)
        .SetPayload(Route::Test::Message)
        .ValidatedBuild();
    ASSERT_TRUE(optMessage);

    Peer::Action::Next next{ m_spProxy, *optMessage, m_spServiceProvider };

    // The router should propogate the handler's failure to handle a message. 
    EXPECT_FALSE(m_router.Route(*optMessage, next));
}

//----------------------------------------------------------------------------------------------------------------------

bool test::InspectableHandler::OnFetchServices(std::shared_ptr<Node::ServiceProvider> const& spServiceProvider)
{
    m_wpInspectableService = spServiceProvider->Fetch<test::InspectableService>();
    if (m_wpInspectableService.expired()) { return false; }

    auto const spInspectable = m_wpInspectableService.lock();
    if (!spInspectable) { return false; }
    spInspectable->OnFetched();

    return true;
}

//----------------------------------------------------------------------------------------------------------------------

bool test::InspectableHandler::OnMessage(Message::Application::Parcel const& message, Peer::Action::Next&)
{
    if (auto const spInspectable = m_wpInspectableService.lock(); spInspectable) {
        spInspectable->OnHandled();
    }
    return message.GetRoute() == test::InspectableRoute;
}

//----------------------------------------------------------------------------------------------------------------------
