//------------------------------------------------------------------------------------------------
// File: MessageQueue.cpp
// Description:
//------------------------------------------------------------------------------------------------
#pragma once
//------------------------------------------------------------------------------------------------
#include "../../Interfaces/MessageSink.hpp"
#include "../../Utilities/Message.hpp"
#include "../../Utilities/NodeUtils.hpp"
//------------------------------------------------------------------------------------------------
#include <cstdio>
#include <fstream>
#include <optional>
#include <string>
#include <unordered_map>
#include <queue>
#include <shared_mutex>
//------------------------------------------------------------------------------------------------

class CMessageQueue : public IMessageSink {
public:
    CMessageQueue();
    ~CMessageQueue();
    
    bool PushOutgoingMessage(
        NodeUtils::NodeIdType id,
        CMessage const& message);
    std::optional<CMessage> PopIncomingMessage();
    
    std::uint32_t QueuedMessages();
    std::uint32_t RegisteredConnections();

    // IMessageSink {
    virtual void ForwardMessage(
        NodeUtils::NodeIdType id,
        CMessage const& message) override;
    virtual void RegisterCallback(
        NodeUtils::NodeIdType id,
        CConnection* context,
        ProcessedMessageCallback callback) override;
    virtual void UnpublishCallback(NodeUtils::NodeIdType id) override;
    // } IMessage Sink

private:
    mutable std::shared_mutex m_incomingMutex;
    mutable std::shared_mutex m_callbacksMutex;

    std::queue<CMessage> m_incoming;

    std::unordered_map<NodeUtils::NodeIdType, ConnectionContextPair> m_callbacks;
};

//------------------------------------------------------------------------------------------------