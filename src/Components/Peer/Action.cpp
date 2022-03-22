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

std::optional<Awaitable::TrackerKey> Peer::Action::Next::Defer(DeferredOptions&& options) const
{
    assert(options.notice.type != Message::Destination::Node);
    assert(!options.notice.route.empty());

    if (auto const& spServiceProvider = m_wpServiceProvider.lock(); spServiceProvider) {
        auto const spNodeState = spServiceProvider->Fetch<NodeState>().lock();
        auto const spPeerManager = spServiceProvider->Fetch<IPeerCache>().lock();
        auto const spTrackingService = spServiceProvider->Fetch<Awaitable::TrackingService>().lock();
        if (!spNodeState || !spPeerManager || !spTrackingService) { return {}; }

        std::vector<Node::SharedIdentifier> identifiers;
        identifiers.emplace_back(spNodeState->GetNodeIdentifier());
        spPeerManager->ForEach([&identifiers] (Node::SharedIdentifier const& spPeerIdentifier) -> CallbackIteration {
            identifiers.emplace_back(spPeerIdentifier);
            return CallbackIteration::Continue;
        });

        auto builder = Message::Application::Parcel::GetBuilder()
            .SetContext(m_message.get().GetContext())
            .SetSource(*spNodeState->GetNodeIdentifier())
            .SetRoute(options.notice.route)
            .SetPayload(std::move(options.notice.payload));
            
        auto const optTrackerKey = spTrackingService->StageDeferred(m_wpProxy, identifiers, m_message.get(), builder);
        if (!optTrackerKey) { return {}; }

        switch (options.notice.type) {
            case Message::Destination::Cluster: builder.MakeClusterMessage(); break;
            case Message::Destination::Network: builder.MakeNetworkMessage(); break;
            default: assert(false); break; // Callers should not call this method for other destination types
        }

        // TODO: Send notice to peers.
        auto const optNotice = builder.ValidatedBuild();
        assert(optNotice);

        [[maybe_unused]] bool const success = spTrackingService->Process(
            *optTrackerKey, *spNodeState->GetNodeIdentifier(), std::move(options.response.payload));
        assert(success);

        return optTrackerKey;
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

bool Peer::Action::Next::Respond(Message::Payload&& payload) const
{
    if (auto const& spServiceProvider = m_wpServiceProvider.lock(); spServiceProvider) {
        auto const spNodeState = spServiceProvider->Fetch<NodeState>().lock();

        auto builder = Message::Application::Parcel::GetBuilder()
            .SetContext(m_message.get().GetContext())
            .SetSource(*spNodeState->GetNodeIdentifier())
            .SetDestination(m_message.get().GetSource())
            .SetRoute(m_message.get().GetRoute())
            .SetPayload(std::move(payload));
        
        using namespace Message::Application;
        if (auto const optAwaitable = m_message.get().GetExtension<Extension::Awaitable>(); optAwaitable) {
            builder.BindExtension<Extension::Awaitable>(
                Extension::Awaitable::Response, optAwaitable->get().GetTracker());
        }

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
