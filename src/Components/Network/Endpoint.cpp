//----------------------------------------------------------------------------------------------------------------------
// File: Endpoint.cpp
// Description: Defines a set of communication methods for use on varying types of communication
// protocols. Currently supports TCP sockets.
//----------------------------------------------------------------------------------------------------------------------
#include "Endpoint.hpp"
#include "Address.hpp"
#include "Components/Configuration/Defaults.hpp"
#include "Components/Event/Publisher.hpp"
#include "Components/Identifier/BryptIdentifier.hpp"
#include "Components/Message/ApplicationMessage.hpp"
#include "Components/Network/LoRa/Endpoint.hpp"
#include "Components/Network/TCP/Endpoint.hpp"
#include "Components/Peer/Proxy.hpp"
//----------------------------------------------------------------------------------------------------------------------

Network::Endpoint::Properties::Properties()
    : m_binding()
    , m_connection()
{
    static_assert(std::in_range<std::uint32_t>(Configuration::Defaults::ConnectionRetryLimit));
    m_connection.m_timeout = Configuration::Defaults::ConnectionTimeout.count();
    m_connection.m_limit = Configuration::Defaults::ConnectionRetryLimit;
    m_connection.m_interval = Configuration::Defaults::ConnectionRetryInterval.count();
}

//----------------------------------------------------------------------------------------------------------------------

Network::Endpoint::Properties::Properties(Configuration::Options::Endpoint const& options)
    : m_binding(options.GetBinding())
    , m_connection()
{
    assert(std::in_range<std::uint32_t>(options.GetConnectionRetryLimit()));
    m_connection.m_timeout = options.GetConnectionTimeout().count();
    m_connection.m_limit = options.GetConnectionRetryLimit();
    m_connection.m_interval = options.GetConnectionRetryInterval().count();
}

//----------------------------------------------------------------------------------------------------------------------

Network::Endpoint::Properties::Properties(Properties const& other)
    : m_binding(other.m_binding)
    , m_connection()
{
    m_connection.m_timeout = other.m_connection.m_timeout.load();
    m_connection.m_limit = other.m_connection.m_limit.load();
    m_connection.m_interval = other.m_connection.m_interval.load();
}

//----------------------------------------------------------------------------------------------------------------------

Network::Endpoint::Properties::Properties(Properties&& other)
    : m_binding(std::move(other.m_binding))
    , m_connection()
{
    m_connection.m_timeout = other.m_connection.m_timeout.load();
    m_connection.m_limit = other.m_connection.m_limit.load();
    m_connection.m_interval = other.m_connection.m_interval.load();
}

//----------------------------------------------------------------------------------------------------------------------

Network::Endpoint::Properties& Network::Endpoint::Properties::operator=(Properties const& other)
{
    m_binding = other.m_binding;
    m_connection.m_timeout = other.m_connection.m_timeout.load();
    m_connection.m_limit = other.m_connection.m_limit.load();
    m_connection.m_interval = other.m_connection.m_interval.load();
    return *this;
}

//----------------------------------------------------------------------------------------------------------------------

Network::Endpoint::Properties& Network::Endpoint::Properties::operator=(Properties&& other)
{
    m_binding = std::move(other.m_binding);
    m_connection.m_timeout = other.m_connection.m_timeout.load();
    m_connection.m_limit = other.m_connection.m_limit.load();
    m_connection.m_interval = other.m_connection.m_interval.load();
    return *this;
}

//----------------------------------------------------------------------------------------------------------------------

Network::Protocol Network::Endpoint::Properties::GetProtocol() const { return m_binding.GetProtocol(); }

//----------------------------------------------------------------------------------------------------------------------

Network::BindingAddress Network::Endpoint::Properties::GetBinding() const { return m_binding; }

//----------------------------------------------------------------------------------------------------------------------

std::chrono::milliseconds Network::Endpoint::Properties::GetConnectionTimeout() const
{
    return std::chrono::milliseconds{ m_connection.m_timeout.load() };
}

//----------------------------------------------------------------------------------------------------------------------

std::uint32_t Network::Endpoint::Properties::GetConnectionRetryLimit() const { return m_connection.m_limit; }

//----------------------------------------------------------------------------------------------------------------------

std::chrono::milliseconds Network::Endpoint::Properties::GetConnectionRetryInterval() const
{
    return std::chrono::milliseconds{ m_connection.m_interval.load() };
}

//----------------------------------------------------------------------------------------------------------------------

void Network::Endpoint::Properties::SetBinding(Network::BindingAddress const& binding) { m_binding = binding; }

//----------------------------------------------------------------------------------------------------------------------

void Network::Endpoint::Properties::SetConnectionTimeout(std::chrono::milliseconds const& value)
{
    m_connection.m_timeout = value.count();
}

//----------------------------------------------------------------------------------------------------------------------

void Network::Endpoint::Properties::SetConnectionRetryLimit(std::int32_t value)
{
    assert(std::in_range<std::uint32_t>(value));
    m_connection.m_limit = value;
}

//----------------------------------------------------------------------------------------------------------------------

void Network::Endpoint::Properties::SetConnectionRetryInterval(std::chrono::milliseconds const& value)
{
    m_connection.m_interval = value.count();
}

//----------------------------------------------------------------------------------------------------------------------

Network::IEndpoint::IEndpoint(Endpoint::Properties const& properties)
    : m_identifier(Endpoint::IdentifierGenerator::Instance().Generate())
    , m_properties(properties)
    , m_spEventPublisher()
    , m_pEndpointMediator(nullptr)
    , m_pResolutionService(nullptr)
    , m_optShutdownCause()
{
}

//----------------------------------------------------------------------------------------------------------------------

Network::Endpoint::Identifier Network::IEndpoint::GetIdentifier() const { return m_identifier; }

//----------------------------------------------------------------------------------------------------------------------

Network::Endpoint::Properties& Network::IEndpoint::GetProperties() { return m_properties; }

//----------------------------------------------------------------------------------------------------------------------

Network::Endpoint::Properties const& Network::IEndpoint::GetProperties() const { return m_properties; }

//----------------------------------------------------------------------------------------------------------------------

void Network::IEndpoint::Register(Event::SharedPublisher const& spPublisher)
{
    assert(!IsActive());
    using enum Event::Type;
    m_spEventPublisher = spPublisher;
    m_spEventPublisher->Advertise({ EndpointStarted, EndpointStopped, BindingFailed, ConnectionFailed });
}

//----------------------------------------------------------------------------------------------------------------------

void Network::IEndpoint::Register(IEndpointMediator* const pMediator)
{
    assert(!IsActive());
    m_pEndpointMediator = pMediator;
}

//----------------------------------------------------------------------------------------------------------------------

void Network::IEndpoint::Register(IResolutionService* const pResolutionService)
{
    assert(!IsActive());
    m_pResolutionService = pResolutionService;
}

//----------------------------------------------------------------------------------------------------------------------

bool Network::IEndpoint::IsStopping() const
{
    return m_optShutdownCause.has_value();
}

//----------------------------------------------------------------------------------------------------------------------

void Network::IEndpoint::OnStarted()
{
    m_optShutdownCause.reset(); // Ensure the shutdown cause has been reset for this cycle.
    m_spEventPublisher->Publish<Event::Type::EndpointStarted>(
        m_identifier, m_properties.GetBinding());
}

//----------------------------------------------------------------------------------------------------------------------

void Network::IEndpoint::OnStopped()
{
    m_spEventPublisher->Publish<Event::Type::EndpointStopped>(
        m_identifier,
        m_properties.GetBinding(),
        m_optShutdownCause.value_or(ShutdownCause::ShutdownRequest));
}

//----------------------------------------------------------------------------------------------------------------------

void Network::IEndpoint::OnBindingUpdated(BindingAddress const& binding)
{
    // If a binding failure was marked as a potential cause of the shutdown, reset the captured shutdown error.
    if (m_optShutdownCause && *m_optShutdownCause == ShutdownCause::BindingFailed) { m_optShutdownCause.reset(); }
    if (m_pEndpointMediator) [[likely]] { m_pEndpointMediator->UpdateBinding(m_identifier, binding); }
}

//----------------------------------------------------------------------------------------------------------------------

void Network::IEndpoint::OnBindingFailed(BindingAddress const& binding, BindingFailure failure) const
{
    SetShutdownCause(ShutdownCause::BindingFailed);
    m_spEventPublisher->Publish<Event::Type::BindingFailed>(m_identifier, binding, failure);
}

//----------------------------------------------------------------------------------------------------------------------

void Network::IEndpoint::OnConnectionFailed(RemoteAddress const& address, ConnectionFailure failure) const
{
    m_spEventPublisher->Publish<Event::Type::ConnectionFailed>(m_identifier, address, failure);
}

//----------------------------------------------------------------------------------------------------------------------

void Network::IEndpoint::OnShutdownRequested() const
{
    SetShutdownCause(ShutdownCause::ShutdownRequest);
}

//----------------------------------------------------------------------------------------------------------------------

void Network::IEndpoint::OnUnexpectedError() const
{
    SetShutdownCause(ShutdownCause::UnexpectedError);
}

//----------------------------------------------------------------------------------------------------------------------

std::shared_ptr<Peer::Proxy> Network::IEndpoint::LinkPeer(
    Node::Identifier const& identifier, RemoteAddress const& address) const
{
    assert(m_pResolutionService);
    // Use the peer mediator to acquire and link a peer proxy for specified node identifier and address to this endpoint. 
    return m_pResolutionService->LinkPeer(identifier, address);
}

//----------------------------------------------------------------------------------------------------------------------

void Network::IEndpoint::SetShutdownCause(ShutdownCause cause) const
{
    if (m_optShutdownCause) { return; } // Don't overwrite the initial value of the shutdown error. 
    m_optShutdownCause = cause;
}

//----------------------------------------------------------------------------------------------------------------------
