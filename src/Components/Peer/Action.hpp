//----------------------------------------------------------------------------------------------------------------------
// File: Action.hpp
// Description: 
//----------------------------------------------------------------------------------------------------------------------
#pragma once
//----------------------------------------------------------------------------------------------------------------------
#include "BryptMessage/MessageDefinitions.hpp"
#include "BryptMessage/Payload.hpp"
#include "Components/Awaitable/Definitions.hpp"
//----------------------------------------------------------------------------------------------------------------------
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
//----------------------------------------------------------------------------------------------------------------------

namespace Node { class ServiceProvider; }
namespace Message::Application { class Parcel; }

//----------------------------------------------------------------------------------------------------------------------
namespace Peer {
//----------------------------------------------------------------------------------------------------------------------

class Proxy;

//----------------------------------------------------------------------------------------------------------------------
namespace Action {
//----------------------------------------------------------------------------------------------------------------------

class Next;

enum class Error : std::uint32_t { UnexpectedError, Expired };

using OnResponse = std::function<void(Message::Application::Parcel const& response)>;
using OnMessage = std::function<void(Message::Application::Parcel const& message, Next& next)>;
using OnError = std::function<void(Error error)>;

//----------------------------------------------------------------------------------------------------------------------
} // Action namespace
} // Peer namespace
//----------------------------------------------------------------------------------------------------------------------

class Peer::Action::Next
{
public:
    struct DeferredOptions { 
        struct Notice { 
            Message::Destination type;
            std::string_view route;
            Message::Payload payload;
        };

        struct Response { 
            Message::Payload payload;
        };

        Notice notice;
        Response response;
    };

    Next(
        std::weak_ptr<Proxy> const& wpProxy,
        std::reference_wrapper<Message::Application::Parcel const> const& message,
        std::weak_ptr<Node::ServiceProvider> const& wpServiceProvider);

    [[nodiscard]] std::weak_ptr<Proxy> const& GetProxy() const;
    [[nodiscard]] std::optional<Awaitable::TrackerKey> const& GetTrackerKey() const;

    [[nodiscard]] std::optional<Awaitable::TrackerKey> Defer(DeferredOptions&& options);
    [[nodiscard]] bool Dispatch(std::string_view route, Message::Payload&& payload = {}) const;
    [[nodiscard]] bool Respond(Message::Payload&& payload = {}) const;

private:
    [[nodiscard]] bool ScheduleSend(Message::Application::Parcel const& message) const;

    std::weak_ptr<Proxy> m_wpProxy;
    std::reference_wrapper<Message::Application::Parcel const> m_message;
    std::weak_ptr<Node::ServiceProvider> m_wpServiceProvider;

    std::optional<Awaitable::TrackerKey> m_optTrackerKey;
};

//----------------------------------------------------------------------------------------------------------------------
