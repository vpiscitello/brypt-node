//------------------------------------------------------------------------------------------------
// File: MessageCollector.hpp
// Description: Defines an interface that allows endpoints to forward and receive 
// messages from the central message queue.
//------------------------------------------------------------------------------------------------
#pragma once
//------------------------------------------------------------------------------------------------
#include "../Utilities/NodeUtils.hpp"
//------------------------------------------------------------------------------------------------
#include <algorithm>
#include <string_view>
#include <utility>
//------------------------------------------------------------------------------------------------

class CEndpoint;
class CMessage;

//------------------------------------------------------------------------------------------------

using ProcessedMessageCallback = std::function<void(CEndpoint*,NodeUtils::NodeIdType id, CMessage const& message)>;
using PeerContextPair = std::pair<CEndpoint*, ProcessedMessageCallback>;

//------------------------------------------------------------------------------------------------

class IMessageSink
{
public:
    virtual ~IMessageSink() = default;

    virtual void ForwardMessage(
        NodeUtils::NodeIdType id,
        CMessage const& message) = 0;
    virtual void RegisterCallback(
        NodeUtils::NodeIdType id,
        CEndpoint* context,
        ProcessedMessageCallback) = 0;
    virtual void UnpublishCallback(NodeUtils::NodeIdType id) = 0;
};

//------------------------------------------------------------------------------------------------