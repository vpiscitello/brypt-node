//------------------------------------------------------------------------------------------------
// File: MessageSink.hpp
// Description: Defines an interface that allows endpoints to forward and receive 
// messages from the central message queue.
//------------------------------------------------------------------------------------------------
#pragma once
//------------------------------------------------------------------------------------------------
#include "../Components/Endpoints/EndpointIdentifier.hpp"
//------------------------------------------------------------------------------------------------
#include <functional>
#include <utility>
//------------------------------------------------------------------------------------------------

namespace BryptIdentifier {
    class CContainer;
}

class CEndpoint;
class CMessage;

//------------------------------------------------------------------------------------------------

using ProcessedMessageCallback = std::function<bool(CMessage const& message)>;

//------------------------------------------------------------------------------------------------

class IMessageSink
{
public:
    virtual ~IMessageSink() = default;

    virtual void ForwardMessage(CMessage const& message) = 0;

    // To receive messages from the application, an endpoint must register with its identifier and 
    // a callback to field these messages.
    virtual void RegisterCallback(Endpoints::EndpointIdType id, ProcessedMessageCallback callback) = 0;
    virtual void UnpublishCallback(Endpoints::EndpointIdType id) = 0;

    // To publish a valid connection that accepts message forwarding, the endpoint will notifiy
    // the IMessageSink of its ID and the peer's ID. This context will be used to identify which
    // endpoint the message for the node is commincated over.
    virtual void PublishPeerConnection(
        Endpoints::EndpointIdType endpointIdentifier,
        BryptIdentifier::CContainer const& peerIdentifier) = 0;
    virtual void UnpublishPeerConnection(
        Endpoints::EndpointIdType endpointIdentifier,
        BryptIdentifier::CContainer const& peerIdentifier) = 0;
};

//------------------------------------------------------------------------------------------------