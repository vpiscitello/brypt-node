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

//------------------------------------------------------------------------------------------------

using ProcessedMessageCallback = std::function<void(CConnection*, std::string_view message)>;
using ConnectionContextPair = std::pair<CConnection*, ProcessedMessageCallback>;

//------------------------------------------------------------------------------------------------

class IMessageSink
{
public:
    IMessageSink() {};

    virtual void ForwardMessage(
        NodeUtils::NodeIdType id,
        std::string_view message) = 0;
    virtual void RegisterCallback(
        NodeUtils::NodeIdType id,
        CConnection* context,
        ProcessedMessageCallback) = 0;
};

//------------------------------------------------------------------------------------------------