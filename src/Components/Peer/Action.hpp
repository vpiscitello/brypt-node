//----------------------------------------------------------------------------------------------------------------------
// File: Action.hpp
// Description: 
//----------------------------------------------------------------------------------------------------------------------
#pragma once
//----------------------------------------------------------------------------------------------------------------------
#include "Components/Awaitable/Definitions.hpp"
#include "Components/Identifier/IdentifierTypes.hpp"
#include "Components/Message/Extension.hpp"
#include "Components/Message/MessageDefinitions.hpp"
#include "Components/Message/Payload.hpp"
#include "Components/Network/Protocol.hpp"
#include "Utilities/InvokeContext.hpp"
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
class Response;

using OnMessage = std::function<void(Message::Application::Parcel const&, Next&)>;

using OnResponse = std::function<void(Response const&)>;
using OnError = std::function<void(Response const&)>;

//using OnResponse = std::function<void(
//    Awaitable::TrackerKey const&,
//    Message::Application::Parcel const&,
//    std::size_t)>;
//
//using OnError = std::function<void(
//    Awaitable::TrackerKey const&,
//    Node::Identifier const&,
//    Message::Extension::Status::Code,
//    std::size_t)>;

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

    Next(Next const& other) = delete;
    Next(Next&& other) = delete;
    Next& operator=(Next const& other) = delete;
    Next& operator=(Next&& other) = delete;

    [[nodiscard]] std::weak_ptr<Proxy> const& GetProxy() const;
    [[nodiscard]] std::optional<Awaitable::TrackerKey> const& GetTrackerKey() const;

    [[nodiscard]] std::optional<Awaitable::TrackerKey> Defer(DeferredOptions&& options);
    [[nodiscard]] bool Dispatch(std::string_view route, Message::Payload&& payload = {}) const;
    [[nodiscard]] bool Respond(Message::Extension::Status::Code statusCode) const;
    [[nodiscard]] bool Respond(Message::Payload const& payload, Message::Extension::Status::Code statusCode) const;
    [[nodiscard]] bool Respond(Message::Payload&& payload, Message::Extension::Status::Code statusCode) const;

private:
    [[nodiscard]] bool ScheduleSend(Message::Application::Parcel const& message) const;

    std::weak_ptr<Proxy> m_wpProxy;
    std::reference_wrapper<Message::Application::Parcel const> m_message;
    std::weak_ptr<Node::ServiceProvider> m_wpServiceProvider;

    std::optional<Awaitable::TrackerKey> m_optTrackerKey;
};

//----------------------------------------------------------------------------------------------------------------------

class Peer::Action::Response
{
public:
    using TrackerReference = std::reference_wrapper<Awaitable::TrackerKey const>;
    using IdentifierReference = std::reference_wrapper<Node::Identifier const>;
    using MessageReference = std::reference_wrapper<Message::Application::Parcel const>;

    Response(
        TrackerReference const& trackerKey,
        MessageReference const& message,
        Message::Extension::Status::Code statusCode,
        std::size_t remaining);

    Response(
        TrackerReference const& trackerKey,
        IdentifierReference const& identifier,
        Message::Extension::Status::Code statusCode,
        std::size_t remaining);

    Response(Response const& other) = delete;
    Response(Response&& other) = delete;
    Response& operator=(Response const& other) = delete;
    Response& operator=(Response&& other) = delete;

    [[nodiscard]] Awaitable::TrackerKey const& GetTrackerKey() const;
    [[nodiscard]] Node::Identifier const& GetSource() const;
    [[nodiscard]] Message::Payload const& GetPayload() const;
    [[nodiscard]] Network::Protocol GetEndpointProtocol() const;
    [[nodiscard]] Message::Extension::Status::Code GetStatusCode() const;
    [[nodiscard]] bool HasErrorCode() const;
    [[nodiscard]] std::size_t GetRemaining() const;

    UT_SupportMethod(Message::Application::Parcel const& GetUnderlyingMessage() const);

private:
    TrackerReference m_trackerKey;
    IdentifierReference m_identifier;
    std::optional<MessageReference> m_optMessage;
    Message::Extension::Status::Code m_statusCode;
    std::size_t m_remaining;
};

//----------------------------------------------------------------------------------------------------------------------
