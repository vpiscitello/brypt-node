//------------------------------------------------------------------------------------------------
// File: MessageControl.hpp
// Description:
//------------------------------------------------------------------------------------------------
#pragma once
//------------------------------------------------------------------------------------------------
#include "AssociatedMessage.hpp"
#include "../../Interfaces/MessageSink.hpp"
//------------------------------------------------------------------------------------------------
#include <cstdint>
#include <memory>
#include <optional>
#include <queue>
#include <shared_mutex>
//------------------------------------------------------------------------------------------------

class CBryptPeer;
class CMessage;

//------------------------------------------------------------------------------------------------

class CMessageCollector : public IMessageSink {
public:
    CMessageCollector();
    
    std::optional<AssociatedMessage> PopIncomingMessage();
    std::uint32_t QueuedMessageCount() const;

    // IMessageSink {
    virtual void CollectMessage(
        std::weak_ptr<CBryptPeer> const& wpBryptPeer,
        CMessage const& message) override;
    // }IMessageSink

private:
    mutable std::shared_mutex m_incomingMutex;
    std::queue<AssociatedMessage> m_incoming;

};

//------------------------------------------------------------------------------------------------
