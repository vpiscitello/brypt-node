//----------------------------------------------------------------------------------------------------------------------
// File: Session.hpp
// Description: Declaration of the TCP Session.
//----------------------------------------------------------------------------------------------------------------------
#pragma once
//----------------------------------------------------------------------------------------------------------------------
#include "AsioUtils.hpp"
#include "SignalService.hpp"
#include "Components/Identifier/IdentifierTypes.hpp"
#include "Components/Message/ShareablePack.hpp"
#include "Components/Network/Actions.hpp"
#include "Components/Network/Address.hpp"
#include "Components/Network/EndpointTypes.hpp"
//----------------------------------------------------------------------------------------------------------------------
#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/steady_timer.hpp>
#include <spdlog/common.h>
//----------------------------------------------------------------------------------------------------------------------
#include <atomic>
#include <cstdint>
#include <deque>
#include <functional>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <variant>
//----------------------------------------------------------------------------------------------------------------------

namespace spdlog { class logger; }

//----------------------------------------------------------------------------------------------------------------------
namespace Network::TCP {
//----------------------------------------------------------------------------------------------------------------------

class Session;
using SharedSession = std::shared_ptr<Session>;

//----------------------------------------------------------------------------------------------------------------------
} // Network::TCP namespace
//----------------------------------------------------------------------------------------------------------------------

class Network::TCP::Session : public std::enable_shared_from_this<Session>
{
public:
    enum class Event : std::uint32_t { Receive, Stop };
    enum class StopCause : std::uint32_t { Requested, Closed, UnexpectedError };

    template <Event EventName> struct Callback;
    using ReceiveCallback = std::function<
        bool(SharedSession const&, Node::Identifier const&, std::span<std::uint8_t const>)>;
    using StopCallback = std::function<void(SharedSession const&)>;
    
    Session(boost::asio::io_context& context, std::shared_ptr<spdlog::logger> const& logger);
    ~Session();

    [[nodiscard]] bool IsActive() const;
    [[nodiscard]] RemoteAddress const& GetAddress() const;
    [[nodiscard]] StopCause GetStopCause() const;
    [[nodiscard]] boost::asio::ip::tcp::socket& GetSocket();

    template <Event EventName>
    void Subscribe(typename Callback<EventName>::Trait const& callback);

    void Initialize(RemoteAddress::Origin origin, bool bootstrappable);
    void Start();
    void Stop();
    
    [[nodiscard]] bool ScheduleSend(Network::MessageVariant&& message);

private:
    using SessionInstance = Session&;

    struct Receiver {
        using ReceiveResult = boost::asio::awaitable<std::optional<std::size_t>>;
        
        explicit Receiver(SessionInstance instance);
        [[nodiscard]] SocketProcessor operator()();
        [[nodiscard]] ReceiveResult ReceiveHeader();
        [[nodiscard]] ReceiveResult ReceiveMessage(std::size_t processed);
        [[nodiscard]] bool OnReceived(std::span<std::uint8_t const> receivable);
        [[nodiscard]] CompletionOrigin OnReceiveError(boost::system::error_code const& error);

        SessionInstance m_instance;
        boost::system::error_code m_error;

        std::vector<std::uint8_t> m_buffer;
        std::vector<std::uint8_t> m_message;
    };

    struct Dispatcher {
        using DispatcherInstance = Dispatcher&;
        using Switchboard = std::deque<Network::MessageVariant>;

        class Dispatchable;

        explicit Dispatcher(SessionInstance instance);
        [[nodiscard]] SocketProcessor operator()();
        [[nodiscard]] bool ScheduleSend(Network::MessageVariant&& spSharedPack);

        SessionInstance m_instance;
        Switchboard m_switchboard;
        ExclusiveSignalService m_signal;
        boost::system::error_code m_error;
    };
    
    [[nodiscard]] bool OnReceived(Node::Identifier const& identifier, std::span<std::uint8_t const> message);

    void OnPeerDisconnected();
    void OnUnexpectedError(std::string_view error);
    [[nodiscard]] CompletionOrigin OnSocketError(boost::system::error_code const& error);
    void OnSocketError(spdlog::level::level_enum level, std::string_view error, StopCause cause);

    void ResetResources();

    std::shared_ptr<spdlog::logger> m_logger;
    std::atomic_bool m_active;
    boost::asio::ip::tcp::socket m_socket;
    RemoteAddress m_address;

    Receiver m_receiver;
    Dispatcher m_dispatcher;

    ReceiveCallback m_onReceived;
    StopCallback m_onStopped;
    std::optional<StopCause> m_optStopCause;
};

//----------------------------------------------------------------------------------------------------------------------
// Session Event Specialization 
//----------------------------------------------------------------------------------------------------------------------
namespace Network::TCP {
//----------------------------------------------------------------------------------------------------------------------

template<>
struct Session::Callback<Session::Event::Receive> { using Trait = ReceiveCallback; };

template<>
inline void Session::Subscribe<Session::Event::Receive>(
    Callback<Event::Receive>::Trait const& callback) { m_onReceived = callback; }

//----------------------------------------------------------------------------------------------------------------------

template<>
struct Session::Callback<Session::Event::Stop> { using Trait = StopCallback; };

template<>
inline void Session::Subscribe<Session::Event::Stop>(
    Callback<Event::Stop>::Trait const& callback) { m_onStopped = callback; }

//----------------------------------------------------------------------------------------------------------------------
} // Network::TCP namespace
//----------------------------------------------------------------------------------------------------------------------
