//----------------------------------------------------------------------------------------------------------------------
// File: MessageHandler.hpp
// Description: 
//----------------------------------------------------------------------------------------------------------------------
#pragma once
//----------------------------------------------------------------------------------------------------------------------
#include <cstdint>
#include <memory>
//----------------------------------------------------------------------------------------------------------------------

namespace spdlog { class logger; }

namespace Peer { 
    class Proxy;
    namespace Action { class Next; }
}

namespace Node { class ServiceProvider; }
namespace Message::Application { class Parcel; }

//----------------------------------------------------------------------------------------------------------------------
namespace Route {
//----------------------------------------------------------------------------------------------------------------------

class IMessageHandler;

//----------------------------------------------------------------------------------------------------------------------
} // Route namespace
//----------------------------------------------------------------------------------------------------------------------

class Route::IMessageHandler
{
public:
    IMessageHandler();
    virtual ~IMessageHandler() = default;

    [[nodiscard]] virtual bool OnFetchServices(std::shared_ptr<Node::ServiceProvider> const& spServiceProvider) = 0;
    [[nodiscard]] virtual bool OnMessage(Message::Application::Parcel const& message, Peer::Action::Next& next) = 0;

protected:
    std::shared_ptr<spdlog::logger> m_logger;
};

//----------------------------------------------------------------------------------------------------------------------
