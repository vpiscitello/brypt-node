//----------------------------------------------------------------------------------------------------------------------
// File: Endpoint.hpp
// Description: Defines a set of communication methods for use on varying types of communication
// protocols. Currently supports TCP sockets.
//----------------------------------------------------------------------------------------------------------------------
#pragma once
//----------------------------------------------------------------------------------------------------------------------
#include "Actions.hpp"
#include "EndpointTypes.hpp"
#include "EndpointIdentifier.hpp"
#include "Protocol.hpp"
#include "BryptIdentifier/BryptIdentifier.hpp"
#include "BryptMessage/ShareablePack.hpp"
#include "Components/Configuration/Options.hpp"
#include "Components/Event/Events.hpp"
#include "Components/Event/SharedPublisher.hpp"
#include "Interfaces/EndpointMediator.hpp"
#include "Interfaces/ResolutionService.hpp"
//----------------------------------------------------------------------------------------------------------------------
#include <chrono>
#include <memory>
#include <string>
#include <string_view>
//----------------------------------------------------------------------------------------------------------------------

namespace Peer { class Proxy; }

namespace Network {
    class IEndpoint;
    class Address;
}

namespace Network::LoRa { class Endpoint; }
namespace Network::TCP { class Endpoint; }

//----------------------------------------------------------------------------------------------------------------------
namespace Network::Endpoint {
//----------------------------------------------------------------------------------------------------------------------

class Properties;

//----------------------------------------------------------------------------------------------------------------------
} // Network::Endpoint namespace
//----------------------------------------------------------------------------------------------------------------------

class Network::Endpoint::Properties
{
public:
    Properties();
    explicit Properties(Configuration::Options::Endpoint const& options);
    ~Properties() = default;

    Properties(Properties const& other);
    Properties(Properties&& other);
    Properties& operator=(Properties const& other);
    Properties& operator=(Properties&& other);

    [[nodiscard]] std::strong_ordering operator<=>(Properties const& other) const = default;
    [[nodiscard]] bool operator==(Properties const& other) const = default;

    [[nodiscard]] Protocol GetProtocol() const;
    [[nodiscard]] BindingAddress GetBinding() const;
    [[nodiscard]] std::chrono::milliseconds GetConnectionTimeout() const;
    [[nodiscard]] std::uint32_t GetConnectionRetryLimit() const;
    [[nodiscard]] std::chrono::milliseconds GetConnectionRetryInterval() const;

    void SetBinding(Network::BindingAddress const& binding);

    void SetConnectionTimeout(std::chrono::milliseconds const& value);
    void SetConnectionRetryLimit(std::int32_t value);
    void SetConnectionRetryInterval(std::chrono::milliseconds const& value);

private:
    struct Connection {
        std::atomic_int64_t m_timeout;
        std::atomic_uint32_t m_limit;
        std::atomic_int64_t m_interval;
    };

    Network::BindingAddress m_binding;
    Connection m_connection;
};

//----------------------------------------------------------------------------------------------------------------------

class Network::IEndpoint
{
public:
    explicit IEndpoint(Endpoint::Properties const& properties);
    virtual ~IEndpoint() = default;

    [[nodiscard]] Endpoint::Identifier GetIdentifier() const;
    [[nodiscard]] Endpoint::Properties& GetProperties();
    [[nodiscard]] Endpoint::Properties const& GetProperties() const;

    [[nodiscard]] virtual Protocol GetProtocol() const = 0;
    [[nodiscard]] virtual std::string_view GetScheme() const = 0;
    [[nodiscard]] virtual BindingAddress GetBinding() const = 0;

    virtual void Startup() = 0;
	[[nodiscard]] virtual bool Shutdown() = 0;
    [[nodiscard]] virtual bool IsActive() const = 0;

    [[nodiscard]] virtual bool ScheduleBind(BindingAddress const& binding) = 0;
    [[nodiscard]] virtual bool ScheduleConnect(RemoteAddress const& address) = 0;
    [[nodiscard]] virtual bool ScheduleConnect(RemoteAddress&& address) = 0;
    [[nodiscard]] virtual bool ScheduleConnect(RemoteAddress&& address, Node::SharedIdentifier const& spIdentifier) = 0;
    [[nodiscard]] virtual bool ScheduleDisconnect(RemoteAddress const& address) = 0;
    [[nodiscard]] virtual bool ScheduleDisconnect(RemoteAddress&& address) = 0;
	[[nodiscard]] virtual bool ScheduleSend(Node::Identifier const& destination, std::string&& message) = 0;
    [[nodiscard]] virtual bool ScheduleSend(
        Node::Identifier const& identifier, Message::ShareablePack const& spSharedPack) = 0;
    [[nodiscard]] virtual bool ScheduleSend(Node::Identifier const& identifier, MessageVariant&& message) = 0;

    void Register(Event::SharedPublisher const& spPublisher);
    void Register(IEndpointMediator* const pMediator);
    void Register(IResolutionService* const pResolutionService);

protected: 
    using ShutdownCause = Event::Message<Event::Type::EndpointStopped>::Cause;
    using BindingFailure = Event::Message<Event::Type::BindingFailed>::Cause;
    using ConnectionFailure = Event::Message<Event::Type::ConnectionFailed>::Cause;

    std::shared_ptr<Peer::Proxy> LinkPeer(Node::Identifier const& identifier, RemoteAddress const& address) const;

    [[nodiscard]] bool IsStopping() const;

    virtual void OnStarted();
    virtual void OnStopped();

    void OnBindingUpdated(BindingAddress const& binding);
    void OnBindingFailed(BindingAddress const& binding, BindingFailure failure) const;
    void OnConnectionFailed(RemoteAddress const& address, ConnectionFailure failure) const;
    void OnShutdownRequested() const;
    void OnUnexpectedError() const;
    
    Endpoint::Identifier const m_identifier;
    Endpoint::Properties m_properties;

    Event::SharedPublisher m_spEventPublisher;
    IEndpointMediator* m_pEndpointMediator;
    IResolutionService* m_pResolutionService;

private:
    void SetShutdownCause(ShutdownCause cause) const;

    // Note: This is mutable because we shouldn't prevent capturing an error that occurs in a const method. 
    mutable std::optional<ShutdownCause> m_optShutdownCause;
};

//----------------------------------------------------------------------------------------------------------------------
