//------------------------------------------------------------------------------------------------
#include "Session.hpp"
#include "EndpointDefinitions.hpp"
#include "BryptMessage/MessageHeader.hpp"
#include "BryptMessage/MessageUtils.hpp"
#include "Utilities/Z85.hpp"
#include "Utilities/NetworkUtils.hpp"
#include "Utilities/NodeUtils.hpp"
//------------------------------------------------------------------------------------------------
#include <boost/asio/buffer.hpp>
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/read.hpp>
#include <boost/asio/redirect_error.hpp>
#include <boost/asio/use_awaitable.hpp>
#include <boost/asio/write.hpp>
//------------------------------------------------------------------------------------------------
#include <cassert>
#include <chrono>
#include <coroutine>
//------------------------------------------------------------------------------------------------

Network::TCP::Session::Session(boost::asio::io_context& context)
    : m_active(false)
    , m_uri()
    , m_socket(context)
    , m_timer(m_socket.get_executor())
    , m_outgoing()
    , m_onDispatched()
    , m_onReceived()
    , m_onError()
{
    m_timer.expires_at(std::chrono::steady_clock::time_point::max());
}

//------------------------------------------------------------------------------------------------

Network::TCP::Session::~Session()
{
    Stop(); // Ensure the socket resources are cleaned up on destruction. 
}

//------------------------------------------------------------------------------------------------

bool Network::TCP::Session::IsActive() const
{
    return m_active;
}

//------------------------------------------------------------------------------------------------

std::string const& Network::TCP::Session::GetURI() const
{
    return m_uri;
}

//------------------------------------------------------------------------------------------------

boost::asio::ip::tcp::socket& Network::TCP::Session::GetSocket()
{
    return m_socket;
}

//------------------------------------------------------------------------------------------------

void Network::TCP::Session::OnMessageDispatched(MessageDispatchedCallback const& callback)
{
    m_onDispatched = callback;
}

//------------------------------------------------------------------------------------------------

void Network::TCP::Session::OnMessageReceived(MessageReceivedCallback const& callback)
{
    m_onReceived = callback;
}

//------------------------------------------------------------------------------------------------

void Network::TCP::Session::OnSessionError(SessionErrorCallback const& callback)
{
    m_onError = callback;
}

//------------------------------------------------------------------------------------------------

void Network::TCP::Session::Initialize()
{
    assert(m_socket.is_open());

    auto const endpoint = m_socket.remote_endpoint();
    std::ostringstream uri;
    uri << Network::TCP::Scheme;
    uri << endpoint.address().to_string() << NetworkUtils::ComponentSeperator << endpoint.port();
    m_uri = uri.str();
}

//------------------------------------------------------------------------------------------------

void Network::TCP::Session::Start()
{
    // Spawn the receiver coroutine.
    boost::asio::co_spawn(
        m_socket.get_executor(),
        [spContext = shared_from_this()] () mutable -> SocketProcessor
        {
            return spContext->Receiver();
        },
        [spContext = shared_from_this()] (std::exception_ptr exception, CompletionOrigin origin)
        {
            bool const error = (exception || origin == CompletionOrigin::Error);
            if (error) { 
                std::ostringstream notification;
                notification << "[TCP] An error caused the receiver for ";
                notification << spContext->GetURI() << " to the shutdown!";
                NodeUtils::printo(notification.str(), NodeUtils::PrintType::Endpoint);
            }
        });

    // Spawn the dispatcher coroutine. 
    boost::asio::co_spawn(
        m_socket.get_executor(),
        [spContext = shared_from_this()] () mutable -> SocketProcessor
        {
            return spContext->Dispatcher();
        },
        [spContext = shared_from_this()] (std::exception_ptr exception, CompletionOrigin origin)
        { 
            bool const error = (exception || origin == CompletionOrigin::Error);
            if (error) { 
                std::ostringstream notification;
                notification << "[TCP] An error caused the dispatcher for ";
                notification << spContext->GetURI() << " to the shutdown!";
                NodeUtils::printo(notification.str(), NodeUtils::PrintType::Endpoint);
            }
        });

    m_active = true;
}

//------------------------------------------------------------------------------------------------

void Network::TCP::Session::Stop()
{
    if (!m_active) [[unlikely]] { assert(!m_socket.is_open()); return; }

    {
        std::ostringstream notification;
        notification << "[TCP] Shutting down session with " << m_uri;
        NodeUtils::printo(notification.str(), NodeUtils::PrintType::Endpoint);
    }

    m_socket.close();
    m_timer.cancel();

    m_active = false;
}

//------------------------------------------------------------------------------------------------

bool Network::TCP::Session::ScheduleSend(std::string_view message)
{
    assert(m_active && m_socket.is_open());
    m_outgoing.emplace_back(message);   // Store the message for the dispatcher coroutine.
    m_timer.cancel_one();   // Wake the dispatcher if it is waiting for data. 
    return true;
}

//------------------------------------------------------------------------------------------------

Network::TCP::SocketProcessor Network::TCP::Session::Receiver()
{
    assert(m_onReceived);

    using ReceiveResult = boost::asio::awaitable<std::optional<std::size_t>>;

    std::vector<std::uint8_t> buffer;
    std::vector<std::uint8_t> message;
    boost::system::error_code error;

    auto const ReceiveHeader = [&, &socket = m_socket] () -> ReceiveResult
    {
        constexpr std::size_t ExpectedHeaderSize = MessageHeader::PeekableEncodedSize();
        // Reserve enough space in the receiving buffer for the message's header.
        buffer.reserve(MessageHeader::PeekableEncodedSize());

        // Receive the message header. 
        std::size_t received = co_await boost::asio::async_read(
            socket,
            boost::asio::dynamic_buffer(buffer, ExpectedHeaderSize),
            boost::asio::redirect_error(boost::asio::use_awaitable, error));

        // If the bytes received does not match the minimum peekable size, an error occured.
        if (error || received != ExpectedHeaderSize) [[unlikely]] { co_return std::nullopt; }

        // Decode the buffer into the message buffer after the header bytes.
        message.resize(Z85::DecodedSize(received));
        Z85::WritableView writable(message.data(), message.size());
        if (!Z85::Decode(buffer, writable)) [[unlikely]] { co_return std::nullopt; }

        // The receive buffe can now be cleared after decoding the message header. 
        buffer.erase(buffer.begin(), buffer.end());

        co_return received;
    };

    auto const ReceiveBody = [&, &socket = m_socket] (std::size_t processed) -> ReceiveResult
    {
        auto const optMessageSize = Message::PeekSize(message);
        if (!optMessageSize) [[unlikely]] { co_return std::nullopt; }

        std::size_t const expected = *optMessageSize - processed;

        // Reserve enough space for the entire message.
        buffer.reserve(*optMessageSize);

        // Receive the rest of the message. 
        std::size_t received = co_await boost::asio::async_read(
            socket,
            boost::asio::dynamic_buffer(buffer, expected),
            boost::asio::redirect_error(boost::asio::use_awaitable, error));

        // If the amount of bytes received does not match the anticipated message size 
        // something went wrong.
        if (error || received != expected) [[unlikely]] { co_return std::nullopt; }
        
        // Decode the buffer into the message buffer after the header bytes.
        auto const size = message.size();
        message.resize(size + Z85::DecodedSize(received));
        Z85::WritableView writable(message.data() + size, message.size() - size);
        if (!Z85::Decode(buffer, writable)) { co_return std::nullopt; }

        buffer.erase(buffer.begin(), buffer.end());

        co_return received;
    };

    auto const OnReceiveError = [this] (boost::system::error_code const& error)
        -> CompletionOrigin
    {
        if (error) {
            return OnSessionError(error);
        } else {
            OnSessionError("A parsing error occured while receiving message");
            return CompletionOrigin::Error;
        }
    };

    while (m_active) {
        auto const optHeaderBytes = co_await ReceiveHeader();
        if (!optHeaderBytes) { co_return OnReceiveError(error);  }

        auto const optMessageBytes = co_await ReceiveBody(*optHeaderBytes);
        if (!optMessageBytes) { co_return OnReceiveError(error); }  

        // Forward the decoded message to the message handler. 
        if (!OnMessageReceived(message)) { co_return CompletionOrigin::Peer; }

        // Erase the current message in preperation for the next message. 
        message.erase(message.begin(), message.end());
    }

    co_return CompletionOrigin::Self;
}

//------------------------------------------------------------------------------------------------

Network::TCP::SocketProcessor Network::TCP::Session::Dispatcher()
{
    boost::system::error_code error;
    assert(m_onDispatched);

    while (m_active) {
        if (m_outgoing.empty()) {
            co_await m_timer.async_wait(
                boost::asio::redirect_error(boost::asio::use_awaitable, error));
        } else {
            [[maybe_unused]] std::size_t sent = co_await boost::asio::async_write(m_socket,
                boost::asio::buffer(m_outgoing.front()),
                boost::asio::redirect_error(boost::asio::use_awaitable, error));
            assert(sent == m_outgoing.front().size());

            if (error) { co_return OnSessionError(error); }

            {
                std::ostringstream notification;
                notification << "[TCP] Sent " << m_outgoing.front() << "!";
                NodeUtils::printo(notification.str(), NodeUtils::PrintType::Endpoint);
            }

            // Notify the message dispatch watcher of a successfully sent message. 
            m_onDispatched(shared_from_this());

            // Remove the message from the outgoing queue, the mem
            m_outgoing.pop_front();
        }
    }

    co_return CompletionOrigin::Self;
}

//------------------------------------------------------------------------------------------------

bool Network::TCP::Session::OnMessageReceived(std::span<std::uint8_t const> receivable)
{
    assert(receivable.size() > 0);

    constexpr auto MessageUpperBound = std::numeric_limits<std::uint32_t>::max();
    if (receivable.size() > MessageUpperBound) [[unlikely]] {
        OnSessionError("A message exceeding the maximum allowable size was received");
        return false;
    }
    
    auto const optIdentifier = Message::PeekSource(receivable);
    if (!optIdentifier) [[unlikely]] {
        OnSessionError("An error occured parsing a received message");
        return false;
    }

    return m_onReceived(shared_from_this(), *optIdentifier, receivable);
}

//------------------------------------------------------------------------------------------------

Network::TCP::CompletionOrigin Network::TCP::Session::OnSessionError(
    boost::system::error_code const& error)
{
    // Determine if the error is expected from an intentional session shutdown.  
    if (IsInducedError(error)) {
        m_onError(shared_from_this()); // Notify the endpoint.
        return CompletionOrigin::Self;
    }

    // Note: The next error handler will handle notifying the endpoint. 
    switch (error.value()) {
        case boost::asio::error::eof: {
            OnSessionError("Connection closed by peer");
            return CompletionOrigin::Peer;
        }
        default: {
            OnSessionError("An unexpected socket error occured");
            return CompletionOrigin::Error;
        }
    }
}

//------------------------------------------------------------------------------------------------

void Network::TCP::Session::OnSessionError(std::string_view error)
{
    assert(error.size() != 0);
    {
        std::ostringstream notification;
        notification << "[TCP] " << error << " during the session with " << m_uri;
        NodeUtils::printo(notification.str(), NodeUtils::PrintType::Endpoint);
    }

    m_onError(shared_from_this()); // Notify the endpoint.
    
    Stop(); // Stop the session processors. 
}

//------------------------------------------------------------------------------------------------
