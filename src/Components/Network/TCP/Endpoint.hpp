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
#include "Components/Network/Actions.hpp"
#include "Components/Network/Address.hpp"
#include "Components/Network/ConnectionDetails.hpp"
#include "Components/Network/ConnectionTracker.hpp"
#include "Components/Network/Endpoint.hpp"
#include "Components/Network/EndpointTypes.hpp"
#include "Components/Network/Protocol.hpp"
//----------------------------------------------------------------------------------------------------------------------
#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/tcp.hpp>
//----------------------------------------------------------------------------------------------------------------------
#include <any>
#include <atomic>
#include <deque>
#include <latch>
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

    explicit Endpoint(Network::Endpoint::Properties const& properties);
    ~Endpoint() override;

    // IEndpoint {
    [[nodiscard]] virtual Network::Protocol GetProtocol() const override;
    [[nodiscard]] virtual std::string_view GetScheme() const override;
    [[nodiscard]] virtual BindingAddress GetBinding() const override;

    virtual void Startup() override;
    [[nodiscard]] virtual bool Shutdown() override;
    [[nodiscard]] virtual bool IsActive() const override;

    [[nodiscard]] virtual bool ScheduleBind(BindingAddress const& binding) override;
    [[nodiscard]] virtual bool ScheduleConnect(RemoteAddress const& address) override;
    [[nodiscard]] virtual bool ScheduleConnect(RemoteAddress&& address) override;
    [[nodiscard]] virtual bool ScheduleConnect(
        RemoteAddress&& address, Node::SharedIdentifier const& spIdentifier) override;
    [[nodiscard]] virtual bool ScheduleDisconnect(RemoteAddress const& address) override;
    [[nodiscard]] virtual bool ScheduleDisconnect(RemoteAddress&& address) override;
    [[nodiscard]] virtual bool ScheduleSend(Node::Identifier const& identifier, std::string&& message) override;
    [[nodiscard]] virtual bool ScheduleSend(
        Node::Identifier const& identifier, Message::ShareablePack const& spSharedPack) override;
    [[nodiscard]] virtual bool ScheduleSend(Node::Identifier const& identifier, MessageVariant&& message) override;
    // } IEndpoint
    
private:
    using EndpointInstance = Endpoint&;
    using ExtendedDetails = ConnectionDetails<void>;

    class Agent;

    void PollContext();
    void ProcessEvents(std::stop_token token);
    void OnBindEvent(BindEvent const& event);
    void OnConnectEvent(ConnectEvent& event);
    void OnDisconnectEvent(DisconnectEvent const& event);
    void OnDispatchEvent(DispatchEvent& event);

    [[nodiscard]] SharedSession CreateSession();

    void OnSessionStarted(SharedSession const& spSession, RemoteAddress::Origin origin, bool bootstrappable);
    void OnSessionStopped(SharedSession const& spSession);

    [[nodiscard]] bool OnMessageReceived(
        SharedSession const& spSession, Node::Identifier const& source, std::span<std::uint8_t const> message);

    [[nodiscard]] ConflictResult IsConflictingAddress(RemoteAddress const& address) const;

    mutable std::shared_mutex m_detailsMutex;

    boost::asio::io_context m_context;
    std::unique_ptr<Agent> m_upAgent;
    SessionTracker m_tracker;
    MessageAction m_messenger;
    DisconnectAction m_disconnector;

    std::shared_ptr<spdlog::logger> m_logger;
};

//----------------------------------------------------------------------------------------------------------------------
