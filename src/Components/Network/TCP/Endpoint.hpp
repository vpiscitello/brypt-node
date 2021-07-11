//----------------------------------------------------------------------------------------------------------------------
// File: Endpoint.hpp
// Description: Declaration of the TCP Endpoint.
//----------------------------------------------------------------------------------------------------------------------
#pragma once
//----------------------------------------------------------------------------------------------------------------------
#include "AsioUtils.hpp"
#include "EndpointDefinitions.hpp"
#include "Events.hpp"
#include "BryptIdentifier/IdentifierTypes.hpp"
#include "Components/Event/SharedPublisher.hpp"
#include "Components/Network/Address.hpp"
#include "Components/Network/ConnectionDetails.hpp"
#include "Components/Network/ConnectionTracker.hpp"
#include "Components/Network/Endpoint.hpp"
#include "Components/Network/EndpointTypes.hpp"
#include "Components/Network/MessageScheduler.hpp"
#include "Components/Network/Protocol.hpp"
//----------------------------------------------------------------------------------------------------------------------
#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/tcp.hpp>
//----------------------------------------------------------------------------------------------------------------------
#include <any>
#include <atomic>
#include <deque>
#include <mutex>
#include <shared_mutex>
#include <span>
#include <stop_token>
#include <string_view>
#include <thread>
//----------------------------------------------------------------------------------------------------------------------

namespace spdlog { class logger; }

//----------------------------------------------------------------------------------------------------------------------
namespace Network::TCP {
//----------------------------------------------------------------------------------------------------------------------

class Endpoint;

class Session;
using SharedSession = std::shared_ptr<Session>;

//----------------------------------------------------------------------------------------------------------------------
} // Network::TCP namespace
//----------------------------------------------------------------------------------------------------------------------

class Network::TCP::Endpoint final : public Network::IEndpoint
{
public:
    using SessionTracker = ConnectionTracker<SharedSession>;

    Endpoint(Operation operation, ::Event::SharedPublisher const& spEventPublisher);
    ~Endpoint() override;

    // IEndpoint{
    [[nodiscard]] virtual Network::Protocol GetProtocol() const override;
    [[nodiscard]] virtual std::string GetScheme() const override;
    [[nodiscard]] virtual BindingAddress GetBinding() const override;

    virtual void Startup() override;
    [[nodiscard]] virtual bool Shutdown() override;
    [[nodiscard]] virtual bool IsActive() const override;

    [[nodiscard]] virtual bool ScheduleBind(BindingAddress const& binding) override;
    [[nodiscard]] virtual bool ScheduleConnect(RemoteAddress const& address) override;
    [[nodiscard]] virtual bool ScheduleConnect(RemoteAddress&& address) override;
    [[nodiscard]] virtual bool ScheduleConnect(
        RemoteAddress&& address, Node::SharedIdentifier const& spIdentifier) override;
    [[nodiscard]] virtual bool ScheduleSend(Node::Identifier const& identifier, std::string&& message) override;
    [[nodiscard]] virtual bool ScheduleSend(
        Node::Identifier const& identifier, Message::ShareablePack const& spSharedPack) override;
    [[nodiscard]] virtual bool ScheduleSend(Node::Identifier const& identifier, MessageVariant&& message) override;
    // }IEndpoint
    
private:
    using EndpointInstance = Endpoint&;
    using ExtendedDetails = ConnectionDetails<void>;

    using EventDeque = std::deque<std::any>;
    using EventHandlers = std::unordered_map<std::type_index, std::function<void(std::any&)>>;

    class Agent {
    public:
        explicit Agent(EndpointInstance endpoint);
        virtual ~Agent() = default;
        [[nodiscard]] virtual Operation Type() const = 0;
        [[nodiscard]] virtual bool IsActive() const;
        [[nodiscard]] bool Launched() const;

    protected:
        void Launch(std::function<void()> const& setup, std::function<void()> const& teardown);
        void Stop();
        virtual void Setup() = 0;
        virtual void Teardown() = 0;

        EndpointInstance m_endpoint;
        std::atomic_bool m_active;
        std::jthread m_worker;
    };

    class Server;
    class Client;

    void PollContext();
    void ProcessEvents(std::stop_token token);
    void OnBindEvent(BindEvent const& event);
    void OnConnectEvent(ConnectEvent& event);
    void OnDispatchEvent(DispatchEvent& event);

    [[nodiscard]] SharedSession CreateSession();

    void OnSessionStarted(SharedSession const& spSession);
    void OnSessionStopped(SharedSession const& spSession);

    [[nodiscard]] bool OnMessageReceived(
        SharedSession const& spSession, Node::Identifier const& source, std::span<std::uint8_t const> message);

    [[nodiscard]] ConnectStatus IsConflictingAddress(RemoteAddress const& address) const;

    mutable std::shared_mutex m_detailsMutex;

    boost::asio::io_context m_context;
    std::unique_ptr<Agent> m_upAgent;
    
    mutable std::mutex m_eventsMutex;
    EventDeque m_events;
    EventHandlers m_handlers;

    SessionTracker m_tracker;
    MessageScheduler m_scheduler;

    std::shared_ptr<spdlog::logger> m_logger;
};

//----------------------------------------------------------------------------------------------------------------------
