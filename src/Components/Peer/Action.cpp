//----------------------------------------------------------------------------------------------------------------------
// File: Action.cpp
// Description:
//----------------------------------------------------------------------------------------------------------------------
#include "Action.hpp"
#include "Proxy.hpp"
#include "BryptIdentifier/ReservedIdentifiers.hpp"
#include "BryptMessage/MessageDefinitions.hpp"
#include "BryptNode/ServiceProvider.hpp"
#include "Components/Awaitable/TrackingService.hpp"
#include "Components/Peer/Proxy.hpp"
#include "Components/State/NetworkState.hpp"
#include "Components/State/NodeState.hpp"
#include "Interfaces/PeerCache.hpp"
//----------------------------------------------------------------------------------------------------------------------
#include <cassert>
//----------------------------------------------------------------------------------------------------------------------

//----------------------------------------------------------------------------------------------------------------------
namespace {
namespace local {
//----------------------------------------------------------------------------------------------------------------------

auto const EmptyPayload = Message::Payload{};

//----------------------------------------------------------------------------------------------------------------------
} // local namespace
} // namespace
//----------------------------------------------------------------------------------------------------------------------

Peer::Action::Next::Next(
    std::weak_ptr<Proxy> const& wpProxy,
    std::reference_wrapper<Message::Application::Parcel const> const& message,
    std::weak_ptr<Node::ServiceProvider> const& wpServiceProvider)
    : m_wpProxy(wpProxy)
    , m_message(message)
    , m_wpServiceProvider(wpServiceProvider)
{
}

//----------------------------------------------------------------------------------------------------------------------

std::weak_ptr<Peer::Proxy> const& Peer::Action::Next::GetProxy() const { return m_wpProxy; }

//----------------------------------------------------------------------------------------------------------------------

std::optional<Awaitable::TrackerKey> const& Peer::Action::Next::GetTrackerKey() const { return m_optTrackerKey; }

//----------------------------------------------------------------------------------------------------------------------

std::optional<Awaitable::TrackerKey> Peer::Action::Next::Defer(DeferredOptions&& options)
{
    assert(options.notice.type != Message::Destination::Node);
    assert(!options.notice.route.empty());

    if (auto const& spServiceProvider = m_wpServiceProvider.lock(); spServiceProvider) {
        auto const spNodeState = spServiceProvider->Fetch<NodeState>().lock();
        auto const spPeerCache = spServiceProvider->Fetch<IPeerCache>().lock();
        auto const spTrackingService = spServiceProvider->Fetch<Awaitable::TrackingService>().lock();
        if (!spNodeState || !spPeerCache || !spTrackingService) { return {}; }

        std::vector<Node::SharedIdentifier> identifiers;
        identifiers.emplace_back(spNodeState->GetNodeIdentifier());
        spPeerCache->ForEach([&identifiers] (Node::SharedIdentifier const& spPeerIdentifier) -> CallbackIteration {
            identifiers.emplace_back(spPeerIdentifier);
            return CallbackIteration::Continue;
        });

        auto builder = Message::Application::Parcel::GetBuilder()
            .SetContext(m_message.get().GetContext())
            .SetSource(*spNodeState->GetNodeIdentifier())
            .SetRoute(options.notice.route)
            .SetPayload(std::move(options.notice.payload));
            
        m_optTrackerKey = spTrackingService->StageDeferred(m_wpProxy, identifiers, m_message.get(), builder);
        if (!m_optTrackerKey) { return {}; }

        switch (options.notice.type) {
            case Message::Destination::Cluster: builder.MakeClusterMessage(); break;
            case Message::Destination::Network: builder.MakeNetworkMessage(); break;
            default: assert(false); break; // Callers should not call this method for other destination types
        }

        // TODO: Send notice to peers.
        auto const optNotice = builder.ValidatedBuild();
        assert(optNotice);

        [[maybe_unused]] bool const success = spTrackingService->Process(
            *m_optTrackerKey, *spNodeState->GetNodeIdentifier(), std::move(options.response.payload));
        assert(success);

        return m_optTrackerKey;
    }

    return {};
}

//----------------------------------------------------------------------------------------------------------------------

bool Peer::Action::Next::Dispatch(std::string_view route, Message::Payload&& payload) const
{
    if (auto const& spServiceProvider = m_wpServiceProvider.lock(); spServiceProvider) {
        auto const spNodeState = spServiceProvider->Fetch<NodeState>().lock();

        assert(!route.empty());
        auto builder = Message::Application::Parcel::GetBuilder()
            .SetContext(m_message.get().GetContext())
            .SetSource(*spNodeState->GetNodeIdentifier())
            .SetDestination(m_message.get().GetSource())
            .SetRoute(route)
            .SetPayload(std::move(payload));

        if (auto const optResponse = builder.ValidatedBuild(); optResponse) { 
            return ScheduleSend(*optResponse);
        }
    }

    return false; 
}

//----------------------------------------------------------------------------------------------------------------------

bool Peer::Action::Next::Respond(Message::Extension::Status::Code statusCode) const
{
    return Respond(Message::Payload{ }, statusCode);
}

//----------------------------------------------------------------------------------------------------------------------

bool Peer::Action::Next::Respond(Message::Payload const& payload, Message::Extension::Status::Code statusCode) const
{
    return Respond(Message::Payload{ payload }, statusCode);
}

//----------------------------------------------------------------------------------------------------------------------

bool Peer::Action::Next::Respond(Message::Payload&& payload, Message::Extension::Status::Code statusCode) const
{
    if (auto const& spServiceProvider = m_wpServiceProvider.lock(); spServiceProvider) {
        using namespace Message;
        auto const optAwaitable = m_message.get().GetExtension<Extension::Awaitable>(); 
        if (!optAwaitable || optAwaitable->get().GetBinding() != Extension::Awaitable::Request) { return false; }

        auto const spNodeState = spServiceProvider->Fetch<NodeState>().lock();

        auto builder = Message::Application::Parcel::GetBuilder()
            .SetContext(m_message.get().GetContext())
            .SetSource(*spNodeState->GetNodeIdentifier())
            .SetDestination(m_message.get().GetSource())
            .SetRoute(m_message.get().GetRoute())
            .SetPayload(std::move(payload))
            .BindExtension<Extension::Awaitable>(Extension::Awaitable::Response, optAwaitable->get().GetTracker())
            .BindExtension<Extension::Status>(statusCode);

        if (auto const optResponse = builder.ValidatedBuild(); optResponse) { 
            return ScheduleSend(*optResponse);
        }
    }

    return false; 
}

//----------------------------------------------------------------------------------------------------------------------

bool Peer::Action::Next::ScheduleSend(Message::Application::Parcel const& message) const
{
    if (auto const spProxy = m_wpProxy.lock(); spProxy) {
        auto const identifier = m_message.get().GetContext().GetEndpointIdentifier();
        return spProxy->ScheduleSend(identifier, message.GetPack());
    }
    return false;
}

//----------------------------------------------------------------------------------------------------------------------

Peer::Action::Response::Response(
    TrackerReference const& trackerKey,
    MessageReference const& message,
    Message::Extension::Status::Code statusCode,
    std::size_t remaining)
    : m_trackerKey(trackerKey)
    , m_identifier(message.get().GetSource())
    , m_optMessage(message)
    , m_statusCode(statusCode)
    , m_remaining(remaining)
{
}

//----------------------------------------------------------------------------------------------------------------------

Peer::Action::Response::Response(
    TrackerReference const& trackerKey,
    IdentifierReference const& identifier,
    Message::Extension::Status::Code statusCode,
    std::size_t remaining)
    : m_trackerKey(trackerKey)
    , m_identifier(identifier)
    , m_optMessage()
    , m_statusCode(statusCode)
    , m_remaining(remaining)
{
}

//----------------------------------------------------------------------------------------------------------------------

Awaitable::TrackerKey const& Peer::Action::Response::GetTrackerKey() const { return m_trackerKey.get(); }

//----------------------------------------------------------------------------------------------------------------------

Node::Identifier const& Peer::Action::Response::GetSource() const { return m_identifier.get(); }

//----------------------------------------------------------------------------------------------------------------------

Message::Payload const& Peer::Action::Response::GetPayload() const
{
    return (m_optMessage) ? m_optMessage->get().GetPayload() : local::EmptyPayload;
}

//----------------------------------------------------------------------------------------------------------------------

Network::Protocol Peer::Action::Response::GetEndpointProtocol() const 
{ 
    return (m_optMessage) ? m_optMessage->get().GetContext().GetEndpointProtocol() : Network::Protocol::Invalid;
}

//----------------------------------------------------------------------------------------------------------------------

Message::Extension::Status::Code Peer::Action::Response::GetStatusCode() const { return m_statusCode; }

//----------------------------------------------------------------------------------------------------------------------

bool Peer::Action::Response::HasErrorCode() const
{
    return Message::Extension::Status::IsErrorCode(m_statusCode);
}

//----------------------------------------------------------------------------------------------------------------------

std::size_t Peer::Action::Response::GetRemaining() const { return m_remaining; }

//----------------------------------------------------------------------------------------------------------------------

template <>
Message::Application::Parcel const& Peer::Action::Response::GetUnderlyingMessage<InvokeContext::Test>() const
{
    return m_optMessage->get();
}

//----------------------------------------------------------------------------------------------------------------------
