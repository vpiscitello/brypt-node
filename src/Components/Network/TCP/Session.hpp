//----------------------------------------------------------------------------------------------------------------------
// File: Session.hpp
// Description: Declaration of the TCP Session.
//----------------------------------------------------------------------------------------------------------------------
#pragma once
//----------------------------------------------------------------------------------------------------------------------
#include "AsioUtils.hpp"
#include "BryptIdentifier/IdentifierTypes.hpp"
#include "Components/Network/Address.hpp"
#include "Components/Network/EndpointTypes.hpp"
//----------------------------------------------------------------------------------------------------------------------
#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/steady_timer.hpp>
//----------------------------------------------------------------------------------------------------------------------
#include <atomic>
#include <cstdint>
#include <deque>
#include <functional>
#include <memory>
#include <span>
#include <string>
#include <string_view>
//----------------------------------------------------------------------------------------------------------------------

namespace spdlog { class logger; }

//----------------------------------------------------------------------------------------------------------------------
namespace Network::TCP {
//----------------------------------------------------------------------------------------------------------------------

class Session;

//----------------------------------------------------------------------------------------------------------------------
} // Network::TCP namespace
//----------------------------------------------------------------------------------------------------------------------

class Network::TCP::Session : public std::enable_shared_from_this<Session>
{
public:
    using MessageReceivedCallback = std::function<bool(
        std::shared_ptr<Session> const&, Node::Identifier const&, std::span<std::uint8_t const> message)>;
    using StoppedCallback = std::function<void(std::shared_ptr<Session> const&)>;
    
    Session(boost::asio::io_context& context, std::shared_ptr<spdlog::logger> const& spLogger);
    ~Session();

    [[nodiscard]] bool IsActive() const;
    [[nodiscard]] RemoteAddress const& GetAddress() const;
    [[nodiscard]] boost::asio::ip::tcp::socket& GetSocket();

    void OnMessageReceived(MessageReceivedCallback const& callback);
    void OnStopped(StoppedCallback const& callback);
    
    void Initialize(Operation source);
    void Start();
    void Stop();
    
    [[nodiscard]] bool ScheduleSend(std::string_view message);

private:
    enum class LogLevel : std::uint8_t { Default, Warning };

    SocketProcessor Receiver();
    SocketProcessor Dispatcher();

    [[nodiscard]] bool OnMessageReceived(std::span<std::uint8_t const> receivable);
    
    [[nodiscard]] CompletionOrigin OnSocketError(boost::system::error_code const& error);
    void OnSocketError(std::string_view error, LogLevel level = LogLevel::Default);

    std::atomic_bool m_active;
    RemoteAddress m_address;

    boost::asio::ip::tcp::socket m_socket;
    boost::asio::steady_timer m_timer;

    std::deque<std::string> m_outgoing;

    MessageReceivedCallback m_onReceived;
    StoppedCallback m_onStopped;

    std::shared_ptr<spdlog::logger> m_spLogger;
};

//----------------------------------------------------------------------------------------------------------------------
