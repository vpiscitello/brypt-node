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
//------------------------------------------------------------------------------------------------

class CMessageQueue : public IMessageSink {
    public:
    	CMessageQueue();
    	~CMessageQueue();
        
    	bool PushOutgoingMessage(
            NodeUtils::NodeIdType id,
            std::string_view message);
    	std::optional<CMessage> PopIncomingMessage();

        // IMessageSink {
        virtual void ForwardMessage(
            NodeUtils::NodeIdType id,
            std::string_view message) override;
        virtual void RegisterCallback(
            NodeUtils::NodeIdType id,
            CConnection* context,
            ProcessedMessageCallback callback) override;
        // } IMessage Sink

    private:
        std::queue<CMessage> m_incoming;

        std::unordered_map<NodeUtils::NodeIdType, ConnectionContextPair> m_callbacks;

        mutable std::mutex m_mutex;

};

//------------------------------------------------------------------------------------------------