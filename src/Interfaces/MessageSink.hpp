//------------------------------------------------------------------------------------------------
// File: MessageCollector.hpp
// Description: Defines an interface that allows connection items to forward and receive 
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

class CConnection;
class CMessage;

//------------------------------------------------------------------------------------------------

using ProcessedMessageCallback = std::function<void(CConnection*, CMessage const& message)>;
using ConnectionContextPair = std::pair<CConnection*, ProcessedMessageCallback>;

//------------------------------------------------------------------------------------------------

class IMessageSink
{
public:
    IMessageSink() {};

    virtual void ForwardMessage(
        NodeUtils::NodeIdType id,
        CMessage const& message) = 0;
    virtual void RegisterCallback(
        NodeUtils::NodeIdType id,
        CConnection* context,
        ProcessedMessageCallback) = 0;
    virtual void UnpublishCallback(NodeUtils::NodeIdType id) = 0;
};

//------------------------------------------------------------------------------------------------