//------------------------------------------------------------------------------------------------
// File: Session.hpp
// Description: Declaration of the TCP Session.
//------------------------------------------------------------------------------------------------
#pragma once
//------------------------------------------------------------------------------------------------
#include "AsioUtils.hpp"
#include "BryptIdentifier/IdentifierTypes.hpp"
//------------------------------------------------------------------------------------------------
#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/steady_timer.hpp>
//------------------------------------------------------------------------------------------------
#include <atomic>
#include <cstdint>
#include <deque>
#include <functional>
#include <memory>
#include <string>
#include <span>
#include <string_view>
//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
namespace Network::TCP {
//------------------------------------------------------------------------------------------------

class Session;

//------------------------------------------------------------------------------------------------
} // TCP namespace
//------------------------------------------------------------------------------------------------

class Network::TCP::Session : public std::enable_shared_from_this<Session>
{
public:
    using MessageDispatchedCallback = std::function<void(std::shared_ptr<Session> const&)>;
    using MessageReceivedCallback = std::function<bool(
        std::shared_ptr<Session> const&,
        BryptIdentifier::Container const&,
        std::span<std::uint8_t const> message)>;
    using SessionErrorCallback = std::function<void(std::shared_ptr<Session> const&)>;
    
    explicit Session(boost::asio::io_context& context);
    ~Session();

    [[nodiscard]] bool IsActive() const;
    [[nodiscard]] std::string const& GetURI() const;
    [[nodiscard]] boost::asio::ip::tcp::socket& GetSocket();

    void OnMessageDispatched(MessageDispatchedCallback const& callback);
    void OnMessageReceived(MessageReceivedCallback const& callback);
    void OnSessionError(SessionErrorCallback const& callback);
    
    void Initialize();
    void Start();
    void Stop();
    
    [[nodiscard]] bool ScheduleSend(std::string_view message);

private:
    SocketProcessor Receiver();
    SocketProcessor Dispatcher();

    [[nodiscard]] bool OnMessageReceived(std::span<std::uint8_t const> receivable);
    
    [[nodiscard]] CompletionOrigin OnSessionError(boost::system::error_code const& error);
    void OnSessionError(std::string_view error);

    std::atomic_bool m_active;
    std::string m_uri;

    boost::asio::ip::tcp::socket m_socket;
    boost::asio::steady_timer m_timer;

    std::deque<std::string> m_outgoing;

    MessageDispatchedCallback m_onDispatched;
    MessageReceivedCallback m_onReceived;
    SessionErrorCallback m_onError;
};

//------------------------------------------------------------------------------------------------
