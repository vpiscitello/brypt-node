//----------------------------------------------------------------------------------------------------------------------
// File: Events.hpp
// Description:
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
#include <boost/preprocessor/seq/for_each_i.hpp>
#include <boost/preprocessor/facilities/empty.hpp>
#include <boost/preprocessor/facilities/va_opt.hpp>
#include <boost/preprocessor/punctuation/comma_if.hpp>
#include <boost/preprocessor/variadic/to_seq.hpp>
//----------------------------------------------------------------------------------------------------------------------
#include <concepts>
#include <cstdint>
#include <functional>
#include <string_view>
#include <tuple>
#include <type_traits>
//----------------------------------------------------------------------------------------------------------------------

namespace Peer { class Proxy; }

//----------------------------------------------------------------------------------------------------------------------
namespace Event {
//----------------------------------------------------------------------------------------------------------------------

enum class Type : std::uint32_t
{
    BindingFailed,
    ConnectionFailed,
    CriticalNetworkFailure,
    EndpointStarted,
    EndpointStopped,
    PeerConnected,
    PeerDisconnected,
    RuntimeStarted,
    RuntimeStopped
};

class IMessage;

// By default, the message constructor is deleted to prevent usage of unspecialized types. 
template<Type SpecificType>
class Message { Message() = delete; }; 

template<Type SpecificType>
concept MessageWithContent = requires { typename Message<SpecificType>::EventContent; };

template<Type SpecificType>
concept MessageWithoutContent = !MessageWithContent<SpecificType>;

//----------------------------------------------------------------------------------------------------------------------
} // Event namespace
//----------------------------------------------------------------------------------------------------------------------

class Event::IMessage
{
public:
    virtual ~IMessage() = default;
    virtual Event::Type GetType() = 0;
};

//----------------------------------------------------------------------------------------------------------------------
// Note: The following macros define the boilerplate  types, methods, and variables that should be present in event 
// specializations. This breakout is done to provide a decent level of flexibility between the traits as well as 
// enable further customization if needed. 
//----------------------------------------------------------------------------------------------------------------------

// Allows the event to define custom causes, (e.g. the event fired because of a defined error).
#define EVENT_MESSAGE_CAUSE(...) \
public: enum class Cause : std::uint32_t { __VA_ARGS__ };

// Converts a callback type into an unqualified content store type (e.g. std::string const& becomes std::string).
#define EVENT_MESSAGE_CONTENT_STORE_TYPES(r, data, i, elem) BOOST_PP_COMMA_IF(i) std::remove_cvref_t<elem>
// Converts a callback type into a named parameter (e.g. std::string const& becomes std::string const& arg_0).
#define EVENT_MESSAGE_CONTENT_STORE_ARGS(r, data, i, elem) BOOST_PP_COMMA_IF(i) elem arg_##i
// Converted a callback type into the name argument (e.g. std::string const& becomes arg_0)
#define EVENT_MESSAGE_CONTENT_STORE_NAMES(r, data, i, elem) BOOST_PP_COMMA_IF(i) arg_##i

// Creates the types, methods, and variables needed to support storing and providing callback content. 
#define EVENT_MESSAGE_CONTENT_STORE(...) \
public:\
    using EventContent = std::tuple<\
        BOOST_PP_SEQ_FOR_EACH_I(EVENT_MESSAGE_CONTENT_STORE_TYPES, _, BOOST_PP_VARIADIC_TO_SEQ(__VA_ARGS__))>; \
    explicit Message(BOOST_PP_SEQ_FOR_EACH_I(\
        EVENT_MESSAGE_CONTENT_STORE_ARGS, _, BOOST_PP_VARIADIC_TO_SEQ(__VA_ARGS__))) \
        : m_content(std::make_tuple(BOOST_PP_SEQ_FOR_EACH_I( \
            EVENT_MESSAGE_CONTENT_STORE_NAMES, _, BOOST_PP_VARIADIC_TO_SEQ(__VA_ARGS__)))) {} \
    explicit Message(EventContent&& content) : m_content(std::move(content)) {} \
    auto const& GetContent() const { return m_content; } \
private: \
    EventContent m_content;
#define EVENT_MESSAGE_CONTENT_STORE_EMPTY \
private: \
    using EventContent = void;

// Creates the core types and methods needed to define an event specialization. 
#define EVENT_MESSAGE_CORE(type, ...) \
public:\
    using CallbackTrait = std::function<void(__VA_ARGS__)>; \
    static constexpr Event::Type Type = type; \
    Message() = default; \
    ~Message() = default; \
    Message(Message const&) = delete; \
    Message(Message&& ) = default; \
    Message& operator=(Message const&) = delete; \
    Message& operator=(Message&&) = default; \
    virtual Event::Type GetType() override { return Type; } \
    BOOST_PP_VA_OPT((EVENT_MESSAGE_CONTENT_STORE(__VA_ARGS__)), (EVENT_MESSAGE_CONTENT_STORE_EMPTY), __VA_ARGS__)
    
//----------------------------------------------------------------------------------------------------------------------
// Schema: The network protocol of the endpoint and the uri.
//----------------------------------------------------------------------------------------------------------------------
template<>
class Event::Message<Event::Type::BindingFailed> : public Event::IMessage
{
    EVENT_MESSAGE_CAUSE(Canceled, AddressInUse, Offline, Unreachable, Permissions, UnexpectedError)
    EVENT_MESSAGE_CORE(
        Event::Type::BindingFailed,
        Network::Endpoint::Identifier, Network::BindingAddress const&, Cause)
};

//----------------------------------------------------------------------------------------------------------------------
// Schema: The network protocol of the endpoint and the uri.
//----------------------------------------------------------------------------------------------------------------------
template<>
class Event::Message<Event::Type::ConnectionFailed> : public Event::IMessage
{
    EVENT_MESSAGE_CAUSE(
        Canceled, InProgress, Duplicate, Reflective, Refused, Offline, Unreachable, Permissions, UnexpectedError)
    EVENT_MESSAGE_CORE(
        Event::Type::ConnectionFailed, Network::Endpoint::Identifier, Network::RemoteAddress const&, Cause)
};

//----------------------------------------------------------------------------------------------------------------------
// Schema: No event data to be captured. 
//----------------------------------------------------------------------------------------------------------------------
template<>
class Event::Message<Event::Type::CriticalNetworkFailure> : public Event::IMessage
{
    EVENT_MESSAGE_CORE(Event::Type::CriticalNetworkFailure)
};

//----------------------------------------------------------------------------------------------------------------------
// Schema: The network protocol of the endpoint and the uri.
//----------------------------------------------------------------------------------------------------------------------
template<>
class Event::Message<Event::Type::EndpointStarted> : public Event::IMessage
{
    EVENT_MESSAGE_CORE(
        Event::Type::EndpointStarted,
        Network::Endpoint::Identifier, Network::BindingAddress)
};

//----------------------------------------------------------------------------------------------------------------------
// Schema: The network protocol of the endpoint, the uri, and the cause of the event.
//----------------------------------------------------------------------------------------------------------------------
template<>
class Event::Message<Event::Type::EndpointStopped> : public Event::IMessage
{
    EVENT_MESSAGE_CAUSE(ShutdownRequest, BindingFailed, UnexpectedError)
    EVENT_MESSAGE_CORE(
        Event::Type::EndpointStopped,
        Network::Endpoint::Identifier, Network::BindingAddress, Cause)
};

//----------------------------------------------------------------------------------------------------------------------
// Schema: The brypt identifier of the peer and the network technology of the endpoint.
//----------------------------------------------------------------------------------------------------------------------
template<>
class Event::Message<Event::Type::PeerConnected> : public Event::IMessage
{
    EVENT_MESSAGE_CORE(
        Event::Type::PeerConnected,
        std::weak_ptr<Peer::Proxy> const&, Network::RemoteAddress const&)
};

//----------------------------------------------------------------------------------------------------------------------
// Schema: The brypt identifier of the peer, the network technology of the endpoint, and the cause of the event.
//----------------------------------------------------------------------------------------------------------------------
template<>
class Event::Message<Event::Type::PeerDisconnected> : public Event::IMessage
{
    EVENT_MESSAGE_CAUSE(DisconnectRequest, SessionClosure, NetworkShutdown, UnexpectedError)
    EVENT_MESSAGE_CORE(
        Event::Type::PeerDisconnected,
        std::weak_ptr<Peer::Proxy> const&, Network::RemoteAddress const&, Cause)
};

//----------------------------------------------------------------------------------------------------------------------
// Schema: No event data to be captured. 
//----------------------------------------------------------------------------------------------------------------------
template<>
class Event::Message<Event::Type::RuntimeStarted> : public Event::IMessage
{
    EVENT_MESSAGE_CORE(Event::Type::RuntimeStarted)
};

//----------------------------------------------------------------------------------------------------------------------
// Schema: The case of the runtime stopping. 
//----------------------------------------------------------------------------------------------------------------------
template<>
class Event::Message<Event::Type::RuntimeStopped> : public Event::IMessage
{
    EVENT_MESSAGE_CAUSE(ShutdownRequest, UnexpectedError)
    EVENT_MESSAGE_CORE(Event::Type::RuntimeStopped, Cause)
};

//----------------------------------------------------------------------------------------------------------------------
