//----------------------------------------------------------------------------------------------------------------------
#pragma once
//----------------------------------------------------------------------------------------------------------------------
#include "BryptIdentifier/BryptIdentifier.hpp"
#include "Components/Network/Address.hpp"
#include "Components/Network/Protocol.hpp"
#include "Components/Network/EndpointIdentifier.hpp"
#include "Components/Network/EndpointTypes.hpp"
#include "Components/Security/Flags.hpp"
//----------------------------------------------------------------------------------------------------------------------
#include <concepts>
#include <cstdint>
#include <functional>
#include <string_view>
#include <tuple>
//----------------------------------------------------------------------------------------------------------------------

//----------------------------------------------------------------------------------------------------------------------
namespace Event {
//----------------------------------------------------------------------------------------------------------------------

enum class Type : std::uint32_t {
    BindingFailed,
    ConnectionFailed,
    CriticalNetworkFailure,
    EndpointStarted,
    EndpointStopped,
    PeerConnected,
    PeerDisconnected,
    RuntimeStarted,
    RuntimeStopped,
};

class IMessage;

template <Type EventType>
class Message;

template<Type EventType>
concept MessageWithContent = requires { typename Message<EventType>::EventContent; };

template<Type EventType>
concept MessageWithoutContent = !MessageWithContent<EventType>;

//----------------------------------------------------------------------------------------------------------------------
} // Event namespace
//----------------------------------------------------------------------------------------------------------------------

class Event::IMessage
{
public:
    virtual ~IMessage() = default;
    virtual Event::Type Type() = 0;
};

//----------------------------------------------------------------------------------------------------------------------

//----------------------------------------------------------------------------------------------------------------------
// Note: Helpful boilerplate macros to define the required class types, methods, and variables that should be present
// in all derived events. Each macro should used in the order listed below. This breakout is done to provide a decent 
// level of flexibility between the content, callback trait, and allow additional methods. 
//----------------------------------------------------------------------------------------------------------------------

#define EVENT_MESSAGE_CAUSE(...)\
public:\
    enum class Cause : std::uint32_t {__VA_ARGS__};

#define EVENT_MESSAGE_CONTENT_STORE(...)\
public:\
    using EventContent = std::tuple<__VA_ARGS__>;\
    explicit Message(EventContent&& content) : m_content(std::move(content)) {}\
    auto const& GetContent() const { return m_content; }\
private:\
    EventContent m_content;

#define EVENT_MESSAGE_CALLBACK_TRAIT(...)\
public:\
    using CallbackTrait = std::function<void(__VA_ARGS__)>;

#define EVENT_MESSAGE_CORE(type)\
public:\
    static constexpr Event::Type EventType = type;\
    Message() = default;\
    ~Message() = default;\
    Message(Message const&) = delete;\
    Message(Message&& ) = default;\
    Message& operator=(Message const&) = delete;\
    Message& operator=(Message&&) = default;\
    virtual Event::Type Type() override { return EventType; }
    
//----------------------------------------------------------------------------------------------------------------------

template <>
class Event::Message<Event::Type::BindingFailed> : public Event::IMessage
{
    // Schema: The network protocol of the endpoint and the uri.
    EVENT_MESSAGE_CONTENT_STORE(Network::Endpoint::Identifier, Network::BindingAddress)
    EVENT_MESSAGE_CALLBACK_TRAIT(Network::Endpoint::Identifier, Network::BindingAddress const&)
    EVENT_MESSAGE_CORE(Event::Type::BindingFailed)
};

//----------------------------------------------------------------------------------------------------------------------

template <>
class Event::Message<Event::Type::ConnectionFailed> : public Event::IMessage
{
    // Schema: The network protocol of the endpoint and the uri.
    EVENT_MESSAGE_CONTENT_STORE(Network::Endpoint::Identifier, Network::RemoteAddress)
    EVENT_MESSAGE_CALLBACK_TRAIT(Network::Endpoint::Identifier, Network::RemoteAddress const&)
    EVENT_MESSAGE_CORE(Event::Type::ConnectionFailed)
};

//----------------------------------------------------------------------------------------------------------------------

template <>
class Event::Message<Event::Type::CriticalNetworkFailure> : public Event::IMessage
{
    // Schema: No event data to be captured. 
    EVENT_MESSAGE_CALLBACK_TRAIT()
    EVENT_MESSAGE_CORE(Event::Type::CriticalNetworkFailure)
};

//----------------------------------------------------------------------------------------------------------------------

template <>
class Event::Message<Event::Type::EndpointStarted> : public Event::IMessage
{
    // Schema: The network protocol of the endpoint and the uri.
    EVENT_MESSAGE_CONTENT_STORE(Network::Endpoint::Identifier, Network::Protocol, Network::Operation)
    EVENT_MESSAGE_CALLBACK_TRAIT(Network::Endpoint::Identifier, Network::Protocol, Network::Operation)
    EVENT_MESSAGE_CORE(Event::Type::EndpointStarted)
};

//----------------------------------------------------------------------------------------------------------------------

template <>
class Event::Message<Event::Type::EndpointStopped> : public Event::IMessage
{
    // Schema: The network protocol of the endpoint, the uri, and the cause of the event.
    EVENT_MESSAGE_CAUSE(ShutdownRequest, BindingFailed, UnexpectedError)
    EVENT_MESSAGE_CONTENT_STORE(Network::Endpoint::Identifier, Network::Protocol, Network::Operation, Cause)
    EVENT_MESSAGE_CALLBACK_TRAIT(Network::Endpoint::Identifier, Network::Protocol, Network::Operation, Cause)
    EVENT_MESSAGE_CORE(Event::Type::EndpointStopped)
};

//----------------------------------------------------------------------------------------------------------------------

template <>
class Event::Message<Event::Type::PeerConnected> : public Event::IMessage
{
    // Schema: The brypt identifier of the peer and the network technology of the endpoint.
    EVENT_MESSAGE_CONTENT_STORE(Network::Protocol, Node::SharedIdentifier)
    EVENT_MESSAGE_CALLBACK_TRAIT( Network::Protocol, Node::SharedIdentifier const&)
    EVENT_MESSAGE_CORE(Event::Type::PeerConnected)
};

//----------------------------------------------------------------------------------------------------------------------

template <>
class Event::Message<Event::Type::PeerDisconnected> : public Event::IMessage
{
    // Schema: The brypt identifier of the peer, the network technology of the endpoint, and the cause of the event.
    EVENT_MESSAGE_CAUSE(SessionClosure, UnexpectedError)
    EVENT_MESSAGE_CONTENT_STORE(Network::Protocol, Node::SharedIdentifier, Cause)
    EVENT_MESSAGE_CALLBACK_TRAIT( Network::Protocol, Node::SharedIdentifier const&, Cause)
    EVENT_MESSAGE_CORE(Event::Type::PeerDisconnected)
};

//----------------------------------------------------------------------------------------------------------------------

template <>
class Event::Message<Event::Type::RuntimeStarted> : public Event::IMessage
{
    // Schema: No event data to be captured. 
    EVENT_MESSAGE_CALLBACK_TRAIT()
    EVENT_MESSAGE_CORE(Event::Type::RuntimeStarted)
};

//----------------------------------------------------------------------------------------------------------------------

template <>
class Event::Message<Event::Type::RuntimeStopped> : public Event::IMessage
{
    // Schema: The case of the runtime stopping. 
    EVENT_MESSAGE_CAUSE(ShutdownRequest, UnexpectedError)
    EVENT_MESSAGE_CONTENT_STORE(Cause)
    EVENT_MESSAGE_CALLBACK_TRAIT(Cause)
    EVENT_MESSAGE_CORE(Event::Type::RuntimeStopped)
};

//----------------------------------------------------------------------------------------------------------------------
