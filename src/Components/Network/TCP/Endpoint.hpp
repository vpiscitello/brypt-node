//------------------------------------------------------------------------------------------------
// File: Endpoint.hpp
// Description: Declaration of the TCP Endpoint.
//------------------------------------------------------------------------------------------------
#pragma once
//------------------------------------------------------------------------------------------------
#include "AsioUtils.hpp"
#include "EndpointDefinitions.hpp"
#include "Events.hpp"
#include "BryptIdentifier/IdentifierTypes.hpp"
#include "Components/Network/Address.hpp"
#include "Components/Network/ConnectionDetails.hpp"
#include "Components/Network/ConnectionTracker.hpp"
#include "Components/Network/Endpoint.hpp"
#include "Components/Network/EndpointTypes.hpp"
#include "Components/Network/MessageScheduler.hpp"
#include "Components/Network/Protocol.hpp"
//------------------------------------------------------------------------------------------------
#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/tcp.hpp>
//------------------------------------------------------------------------------------------------
#include <any>
#include <atomic>
#include <deque>
#include <mutex>
#include <shared_mutex>
#include <span>
#include <stop_token>
#include <string_view>
#include <thread>
//------------------------------------------------------------------------------------------------

namespace spdlog { class logger; }

//------------------------------------------------------------------------------------------------
namespace Network::TCP {
//------------------------------------------------------------------------------------------------

class Session;
class Endpoint;

//------------------------------------------------------------------------------------------------
} // TCP namespace
//------------------------------------------------------------------------------------------------

class Network::TCP::Endpoint : public Network::IEndpoint
{
public:
    using SessionTracker = ConnectionTracker<std::shared_ptr<Session>>;

    explicit Endpoint(Operation operation);
    ~Endpoint() override;

    // IEndpoint{
    [[nodiscard]] virtual Network::Protocol GetProtocol() const override;
    [[nodiscard]] virtual std::string GetScheme() const override;
    [[nodiscard]] virtual BindingAddress GetBinding() const override;

    virtual void Startup() override;
    [[nodiscard]] virtual bool Shutdown() override;
    [[nodiscard]] virtual bool IsActive() const override;

    virtual void ScheduleBind(BindingAddress const& binding) override;

    virtual void ScheduleConnect(RemoteAddress const& address) override;
    virtual void ScheduleConnect(RemoteAddress&& address) override;
    virtual void ScheduleConnect(
        RemoteAddress&& address,
        BryptIdentifier::SharedContainer const& spIdentifier) override;

    [[nodiscard]] virtual bool ScheduleSend(
        BryptIdentifier::Container const& identifier, std::string_view message) override;
    // }IEndpoint
    
private:
    using ExtendedConnectionDetails = ConnectionDetails<void>;

    using EventDeque = std::deque<std::any>;
    using EventProcessors = std::unordered_map<
        std::type_index, std::function<void(std::any const&)>>;

    void ServerWorker(std::stop_token token);    
    void ClientWorker(std::stop_token token);

    void ProcessEvents(std::stop_token token);
    void ProcessBindEvent(BindEvent const& event);
    void ProcessConnectEvent(ConnectEvent const& event);
    void ProcessDispatchEvent(DispatchEvent const& event);

    [[nodiscard]] std::shared_ptr<Session> CreateSession();

    [[nodiscard]] bool Bind(BindingAddress const& binding);
    [[nodiscard]] SocketProcessor ListenProcessor();

    [[nodiscard]] ConnectStatusCode Connect(
        RemoteAddress const& address,
        BryptIdentifier::SharedContainer const& spIdentifier);
    [[nodiscard]] SocketProcessor ConnectionProcessor(
        RemoteAddress address,
        BryptIdentifier::SharedContainer spIdentifier);

    void OnSessionStarted(std::shared_ptr<Session> const& spSession);
    void OnSessionStopped(std::shared_ptr<Session> const& spSession);

    [[nodiscard]] bool OnMessageReceived(
        std::shared_ptr<Session> const& spSession,
        BryptIdentifier::Container const& source,
        std::span<std::uint8_t const> message);
    void OnMessageDispatched(std::shared_ptr<Session> const& spSession);

    mutable std::shared_mutex m_detailsMutex;
	std::atomic_bool m_active;
    std::jthread m_worker;

    boost::asio::io_context m_context;
    boost::asio::ip::tcp::acceptor m_acceptor;
    boost::asio::ip::tcp::resolver m_resolver;
    
    mutable std::mutex m_eventsMutex;
    EventDeque m_events;
    EventProcessors m_processors;

    SessionTracker m_tracker;

    MessageScheduler m_scheduler;

    std::shared_ptr<spdlog::logger> m_spLogger;
};

//------------------------------------------------------------------------------------------------
