//----------------------------------------------------------------------------------------------------------------------
#include "Session.hpp"
//----------------------------------------------------------------------------------------------------------------------
#include "EndpointDefinitions.hpp"
#include "BryptMessage/MessageHeader.hpp"
#include "BryptMessage/MessageUtils.hpp"
#include "Utilities/Z85.hpp"
#include "Utilities/LogUtils.hpp"
//----------------------------------------------------------------------------------------------------------------------
#include <boost/asio/buffer.hpp>
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/read.hpp>
#include <boost/asio/redirect_error.hpp>
#include <boost/asio/use_awaitable.hpp>
#include <boost/asio/write.hpp>
#include <spdlog/fmt/bin_to_hex.h>
#include <spdlog/fmt/fmt.h>
//----------------------------------------------------------------------------------------------------------------------
#include <cassert>
#include <chrono>
#include <coroutine>
//----------------------------------------------------------------------------------------------------------------------

//----------------------------------------------------------------------------------------------------------------------
// Note: The rationale of this nested class is twofold. Firstly, it allows access to underlying message data 
// independent of the ancillary type. The two ancillary types are used to support sending one-off as well as shared
// messages; both are intended to significantly reduce expensive copies that would be required otherwise. Secondly, it
// is designed to manage the lifetime of both the view and real data behind it. The message is fetched and the 
// appropiate queue is popped upon destruction of the view's instance. 
//----------------------------------------------------------------------------------------------------------------------
class Network::TCP::Session::Dispatcher::Dispatchable
{
public:
    using DataReference = std::reference_wrapper<std::string const>;

    [[nodiscard]] static Dispatchable FetchMessage(DispatcherInstance instance) noexcept;
    ~Dispatchable();

    std::string const& get() const { return m_message.get(); }
    std::size_t size() const { return m_message.get().size(); }

private:
    Dispatchable(DispatcherInstance instance,  DataReference const& message);
    DispatcherInstance m_instance;
    DataReference m_message;
};

//----------------------------------------------------------------------------------------------------------------------

Network::TCP::Session::Session(boost::asio::io_context& context, std::shared_ptr<spdlog::logger> const& spLogger)
    : m_spLogger(spLogger)
    , m_active(false)
    , m_socket(context)
    , m_address()
    , m_receiver(*this)
    , m_dispatcher(*this)
    , m_onReceived()
    , m_onStopped()
{
}

//----------------------------------------------------------------------------------------------------------------------

Network::TCP::Session::~Session()
{
    Stop(); // Ensure the socket resources are cleaned up on destruction. 
}

//----------------------------------------------------------------------------------------------------------------------

bool Network::TCP::Session::IsActive() const
{
    return m_active;
}

//----------------------------------------------------------------------------------------------------------------------

Network::RemoteAddress const& Network::TCP::Session::GetAddress() const
{
    return m_address;
}

//----------------------------------------------------------------------------------------------------------------------

boost::asio::ip::tcp::socket& Network::TCP::Session::GetSocket()
{
    return m_socket;
}

//----------------------------------------------------------------------------------------------------------------------

void Network::TCP::Session::Initialize(Operation source)
{
    assert(m_socket.is_open());

    auto const endpoint = m_socket.remote_endpoint();
    std::ostringstream uri;
    uri << Network::TCP::Scheme << Network::SchemeSeperator;
    uri << endpoint.address().to_string() << Network::ComponentSeperator << endpoint.port();

    m_address = { Protocol::TCP, uri.str(), (source == Operation::Client) };
}

//----------------------------------------------------------------------------------------------------------------------

void Network::TCP::Session::Start()
{
    // Spawn the receiver coroutine.
    boost::asio::co_spawn(
        m_socket.get_executor(),
        m_receiver(), // Note: It is assumed that the instance of the session is kept alive externally. 
        [self(shared_from_this()), logger(m_spLogger)] (std::exception_ptr exception, CompletionOrigin origin)
        {
            if (bool const error = (exception || origin == CompletionOrigin::Error); error) { 
                logger->error("An unexpected error caused the receiver for {} to shutdown!", self->GetAddress());
            }
        });

    // Spawn the dispatcher coroutine. 
    boost::asio::co_spawn(
        m_socket.get_executor(),
        m_dispatcher(), // Note: It is assumed that the instance of the session is kept alive externally. 
        [self(shared_from_this()), logger(m_spLogger)] (std::exception_ptr exception, CompletionOrigin origin)
        {
            if (bool const error = (exception || origin == CompletionOrigin::Error); error) { 
                logger->error("An unexpected error caused the receiver for {} to shutdown!", self->GetAddress());
            }
        });

    m_spLogger->info("Session started with {}.", m_address);
    m_active = true;
}

//----------------------------------------------------------------------------------------------------------------------

void Network::TCP::Session::Stop()
{
    if (m_active) { m_spLogger->info("Shutting down session with {}.", m_address); }
    m_socket.close();
    m_dispatcher.m_signal.cancel();
    m_active = false;
}

//----------------------------------------------------------------------------------------------------------------------

bool Network::TCP::Session::ScheduleSend(Network::MessageVariant&& message)
{
    return m_dispatcher.ScheduleSend(std::move(message));
}

//----------------------------------------------------------------------------------------------------------------------

bool Network::TCP::Session::OnReceived(Node::Identifier const& identifier, std::span<std::uint8_t const> message)
{
    return m_onReceived(shared_from_this(), identifier, message);
}

//----------------------------------------------------------------------------------------------------------------------

void Network::TCP::Session::OnPeerDisconnected()
{
    OnSocketError(spdlog::level::warn, "Session ended by peer", StopCause::PeerDisconnect);   
}

//----------------------------------------------------------------------------------------------------------------------

void Network::TCP::Session::OnUnexpectedError(std::string_view error)
{
    OnSocketError(spdlog::level::err, error, StopCause::UnexpectedError);   
}

//----------------------------------------------------------------------------------------------------------------------

Network::TCP::CompletionOrigin Network::TCP::Session::OnSocketError(boost::system::error_code const& error)
{
    // Determine if the error is expected from an intentional session shutdown.  
    if (IsInducedError(error)) {
        m_onStopped(shared_from_this(), StopCause::Requested); // Notify the endpoint.
        return CompletionOrigin::Self;
    }

    // Note: The next error handler will handle notifying the endpoint. 
    switch (error.value()) {
        case boost::asio::error::eof: { OnPeerDisconnected(); } return CompletionOrigin::Peer;
        default: { OnUnexpectedError("An unexpected socket error occured");  } return CompletionOrigin::Error;
    }
}

//----------------------------------------------------------------------------------------------------------------------

void Network::TCP::Session::OnSocketError(spdlog::level::level_enum level, std::string_view error, StopCause cause)
{
    assert(error.size() != 0);
    m_spLogger->log(level, "{} on {}.", error, m_address);
    m_onStopped(shared_from_this(), cause); // Notify the endpoint.
    Stop(); // Stop the session processors. If the socket has already been stopped this is a no-op. 
}

//----------------------------------------------------------------------------------------------------------------------

Network::TCP::Session::Receiver::Receiver(SessionInstance instance)
    : m_instance(instance)
    , m_error()
    , m_buffer()
    , m_message()
{
}

//----------------------------------------------------------------------------------------------------------------------

Network::TCP::SocketProcessor Network::TCP::Session::Receiver::operator()()
{
    auto const& active = m_instance.m_active;
    while (active) {
        auto const optHeaderBytes = co_await ReceiveHeader();
        if (!optHeaderBytes) { co_return OnReceiveError(m_error);  }

        auto const optMessageBytes = co_await ReceiveMessage(*optHeaderBytes);
        if (!optMessageBytes) { co_return OnReceiveError(m_error); }  

        // Forward the decoded message to the message handler. 
        if (!OnReceived(m_message)) { co_return CompletionOrigin::Peer; }

        m_message.clear();
    }

    co_return CompletionOrigin::Self;
}

//----------------------------------------------------------------------------------------------------------------------

Network::TCP::Session::Receiver::ReceiveResult Network::TCP::Session::Receiver::ReceiveHeader()
{
    constexpr std::size_t ExpectedHeaderSize = MessageHeader::PeekableEncodedSize();

    // Reserve enough space in the receiving buffer for the message's header.
    m_buffer.reserve(MessageHeader::PeekableEncodedSize());

    // Receive the message header. 
    std::size_t received = co_await boost::asio::async_read(
        m_instance.m_socket,
        boost::asio::dynamic_buffer(m_buffer, ExpectedHeaderSize),
        boost::asio::redirect_error(boost::asio::use_awaitable, m_error));

    // If the bytes received does not match the minimum peekable size, an error occured.
    if (m_error || received != ExpectedHeaderSize) [[unlikely]] { co_return std::nullopt; }

    // Decode the buffer into the message buffer after the header bytes.
    m_message.resize(Z85::DecodedSize(received));
    Z85::WritableView writable(m_message.data(), m_message.size());
    if (!Z85::Decode(m_buffer, writable)) [[unlikely]] { co_return std::nullopt; }

    co_return received;
}

//----------------------------------------------------------------------------------------------------------------------

Network::TCP::Session::Receiver::ReceiveResult Network::TCP::Session::Receiver::ReceiveMessage(std::size_t processed)
{
    auto const optMessageSize = Message::PeekSize(m_message);
    if (!optMessageSize) [[unlikely]] { co_return std::nullopt; }

    std::size_t const expected = *optMessageSize - processed;
    m_buffer.reserve(*optMessageSize); // Reserve enough space for the entire message.

    // Receive the rest of the message. 
    std::size_t received = co_await boost::asio::async_read(
        m_instance.m_socket,
        boost::asio::dynamic_buffer(m_buffer, *optMessageSize),
        boost::asio::redirect_error(boost::asio::use_awaitable, m_error));

    // Note: This a non-binding request to reduce the capacity of the buffer to current size.
    // This an attempt to prevent memory allocation runaway if when encountering a prior large message. This will 
    // need to be tunned to be smarter (e.g. small messages could cause frequent unnecassery allocations)
    m_buffer.shrink_to_fit();

    // If the amount of bytes received does not match the anticipated message size something went wrong.
    if (m_error || received != expected) [[unlikely]] { co_return std::nullopt; }

    m_instance.m_spLogger->debug("Received {} bytes from {}.", m_buffer.size(), m_instance.m_address);
    m_instance.m_spLogger->trace("[{}] Received: {:p}...", m_instance.m_address,
        spdlog::to_hex(std::string_view(reinterpret_cast<char const*>(
            m_buffer.data()), std::min(m_buffer.size(), MessageHeader::MaximumEncodedSize()))));

    // Decode the buffer into the message buffer after the header bytes.
    auto const size = m_message.size();
    m_message.resize(size + Z85::DecodedSize(received));
    Z85::ReadableView readable(m_buffer.data() + processed, expected);
    Z85::WritableView writable(m_message.data() + size, m_message.size() - size);
    if (!Z85::Decode(readable, writable)) { co_return std::nullopt; }
    m_message.shrink_to_fit(); // See the buffer's "shrink_to_fit" note.
    m_buffer.clear();

    co_return received;
}

//----------------------------------------------------------------------------------------------------------------------

bool Network::TCP::Session::Receiver::OnReceived(std::span<std::uint8_t const> receivable)
{
    assert(receivable.size() > 0);
    constexpr auto MessageUpperBound = std::numeric_limits<std::uint32_t>::max();

    if (receivable.size() > MessageUpperBound) [[unlikely]] {
        m_instance.OnUnexpectedError("Message exceeded the maximum allowable size");
        return false;
    }
    
    auto const optIdentifier = Message::PeekSource(receivable);
    if (!optIdentifier) [[unlikely]] {
        m_instance.OnUnexpectedError("Message was unable to be parsed");
        return false;
    }

    return m_instance.OnReceived(*optIdentifier, receivable);
}

//----------------------------------------------------------------------------------------------------------------------

Network::TCP::CompletionOrigin Network::TCP::Session::Receiver::OnReceiveError(boost::system::error_code const& error)
{
    if (error) { return m_instance.OnSocketError(error); }
    m_instance.OnUnexpectedError("Message was unable to be parsed");
    return CompletionOrigin::Error;
}

//----------------------------------------------------------------------------------------------------------------------

Network::TCP::Session::Dispatcher::Dispatcher(SessionInstance instance)
    : m_instance(instance)
    , m_switchboard()
    , m_signal(instance.m_socket.get_executor())
    , m_error()
{
}

//----------------------------------------------------------------------------------------------------------------------

Network::TCP::SocketProcessor Network::TCP::Session::Dispatcher::operator()()
{
    auto const& active = m_instance.m_active;
    auto const& spLogger = m_instance.m_spLogger;
    auto& socket = m_instance.m_socket;
    while (active) {
        // If there are no messages scheduled for sending, wait for a signal to continue dispatching. 
        if (m_switchboard.empty()) {
            co_await m_signal.async_wait(boost::asio::redirect_error(boost::asio::use_awaitable, m_error));
            if (m_error) { co_return m_instance.OnSocketError(m_error); }
        } else {
            auto const message = Dispatchable::FetchMessage(*this);
            [[maybe_unused]] std::size_t sent = co_await boost::asio::async_write(socket,
                boost::asio::buffer(message.get()), boost::asio::redirect_error(boost::asio::use_awaitable, m_error));
            if (m_error || sent != message.size()) { co_return m_instance.OnSocketError(m_error); }

            spLogger->debug("Dispatched {} bytes to {}.", message.size(), m_instance.m_address);
            spLogger->trace("[{}] Dispatched: {:p}...", m_instance.m_address, spdlog::to_hex(
                std::string_view(message.get().data(), std::min(message.size(), MessageHeader::MaximumEncodedSize()))));
        }
    }

    co_return CompletionOrigin::Self;
}

//----------------------------------------------------------------------------------------------------------------------

bool Network::TCP::Session::Dispatcher::ScheduleSend(Network::MessageVariant&& message)
{
    // Note: There is an expectation that there is only one endpoint scheduling sends for a session. If that assumption
    // is not true, at best the queues become out of sync and at worst memory becomes corrupted. 
    assert(m_instance.m_active && m_instance.m_socket.is_open());
    m_switchboard.emplace_back(std::move(message));   // Store the message for the dispatcher coroutine.
    m_signal.notify();   // Wake the dispatcher if it is waiting for data. 
    return true;
}

//----------------------------------------------------------------------------------------------------------------------

Network::TCP::Session::Dispatcher::Dispatchable Network::TCP::Session::Dispatcher::Dispatchable::FetchMessage(
    DispatcherInstance instance) noexcept
{
    assert(!instance.m_switchboard.empty()); // The switchboard must not be empty to use this helper class. 

    // The method of obtaining the reference to the data is dependent upon the queue to pull from. 
    // If it is a standard string we can use front(), however, for shared messages we must derefence the contents
    // of front(). We must maintain the state of the queues to ensure the lifetime of the messages exist for as long
    // as the wrapper olds the reference. 
    return {
        instance,
        std::holds_alternative<std::string>(instance.m_switchboard.front()) ? 
            std::get<std::string>(instance.m_switchboard.front()) :
            *std::get<Message::ShareablePack>(instance.m_switchboard.front()) };
}

//----------------------------------------------------------------------------------------------------------------------

Network::TCP::Session::Dispatcher::Dispatchable::Dispatchable(DispatcherInstance instance, DataReference const& message)
    : m_instance(instance)
    , m_message(message)
{
}

//----------------------------------------------------------------------------------------------------------------------

Network::TCP::Session::Dispatcher::Dispatchable::~Dispatchable()
{
    m_instance.m_switchboard.pop_front();
}

//----------------------------------------------------------------------------------------------------------------------
