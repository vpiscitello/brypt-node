//------------------------------------------------------------------------------------------------
// File: MessageQueue.cpp
// Description:
//------------------------------------------------------------------------------------------------
#pragma once
//------------------------------------------------------------------------------------------------
#include "../../Utilities/NodeUtils.hpp"
//------------------------------------------------------------------------------------------------
#include <cstdio>
#include <fstream>
#include <optional>
#include <string>
#include <unordered_map>
#include <queue>
//------------------------------------------------------------------------------------------------
class CMessage;
//------------------------------------------------------------------------------------------------

class CMessageQueue {
    public:
    	CMessageQueue();
    	~CMessageQueue();
        
    	bool AddManagedPipe(NodeUtils::NodeIdType const& id);
    	bool RemoveManagedPipe(NodeUtils::NodeIdType const& id);

    	bool PushOutgoingMessage(NodeUtils::NodeIdType const& id, CMessage const& message);
    	std::optional<CMessage> PopIncomingMessage();

        bool CheckPipes();
        bool PushOutgoingMessages();

    private:
        std::queue<CMessage> m_incoming;
        std::queue<CMessage> m_outgoing;

        std::unordered_map<NodeUtils::NodeIdType, std::string> m_pipes;

};

//------------------------------------------------------------------------------------------------