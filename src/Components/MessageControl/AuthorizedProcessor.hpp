//----------------------------------------------------------------------------------------------------------------------
// File: AuthorizedProcessor.hpp
// Description:
//----------------------------------------------------------------------------------------------------------------------
#pragma once
//----------------------------------------------------------------------------------------------------------------------
#include "BryptIdentifier/IdentifierTypes.hpp"
#include "BryptMessage/MessageTypes.hpp"
#include "BryptMessage/ApplicationMessage.hpp"
#include "Components/Route/MessageHandler.hpp"
#include "Interfaces/MessageSink.hpp"
#include "Utilities/InvokeContext.hpp"
#include "Utilities/Logger.hpp"
//----------------------------------------------------------------------------------------------------------------------
#include <cstdint>
#include <memory>
#include <optional>
#include <queue>
#include <shared_mutex>
#include <span>
#include <string_view>
//----------------------------------------------------------------------------------------------------------------------

namespace spdlog { class logger; }

namespace Awaitable { class TrackingService; }
namespace Peer { class Proxy; }
namespace Scheduler { class Delegate; class Registrar; }
namespace Message { class Context; }
namespace Message::Application { class Parcel; }
namespace Message::Platform { class Parcel; }
namespace Route { class Router; }

//----------------------------------------------------------------------------------------------------------------------

class AuthorizedProcessor : public IMessageSink
{
public:
    AuthorizedProcessor(
        std::shared_ptr<Scheduler::Registrar> const& spRegistrar,
        std::shared_ptr<Node::ServiceProvider> const& spServiceProvider);

    ~AuthorizedProcessor();

    // IMessageSink {
    [[nodiscard]] virtual bool CollectMessage(Message::Context const& context, std::string_view buffer) override;
    [[nodiscard]] virtual bool CollectMessage(
        Message::Context const& context, std::span<std::uint8_t const> buffer) override;
    // }IMessageSink
    
    [[nodiscard]] std::size_t MessageCount() const;
    [[nodiscard]] std::size_t Execute();
    
    UT_SupportMethod(std::optional<Message::Application::Parcel> GetNextMessage());

private:
    [[nodiscard]] std::optional<Message::Application::Parcel> FetchMessage();

    [[nodiscard]] bool OnMessageCollected(Message::Application::Parcel&& message);
    [[nodiscard]] bool OnMessageCollected(Message::Platform::Parcel const& message);
        
    std::shared_ptr<spdlog::logger> m_logger;
    std::shared_ptr<Scheduler::Delegate> m_spDelegate;
    Node::SharedIdentifier m_spNodeIdentifier;
    std::shared_ptr<Route::Router> m_spRouter;
    std::shared_ptr<Awaitable::TrackingService> m_spTrackingService;
    std::weak_ptr<Node::ServiceProvider> m_wpServiceProvider;
    
    mutable std::shared_mutex m_mutex;
    std::queue<Message::Application::Parcel> m_incoming;
};

//----------------------------------------------------------------------------------------------------------------------
