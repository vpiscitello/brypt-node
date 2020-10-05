//------------------------------------------------------------------------------------------------
// File: MessageSinkStub.hpp
// Description: An IMessageSink stub implementation for collecting message received through 
// endpoint tests. 
//------------------------------------------------------------------------------------------------
#pragma once
//------------------------------------------------------------------------------------------------
#include "../../BryptMessage/MessageTypes.hpp"
#include "../../Interfaces/MessageSink.hpp"
#include "../../Components/MessageControl/AssociatedMessage.hpp"
//------------------------------------------------------------------------------------------------
#include <cstdint>
#include <memory>
#include <optional>
#include <queue>
#include <shared_mutex>
//------------------------------------------------------------------------------------------------

class CBryptPeer;
class CApplicationMessage;
class CMessageContext;

//------------------------------------------------------------------------------------------------

class CMessageSinkStub : public IMessageSink {
public:
    CMessageSinkStub();
    
    std::optional<AssociatedMessage> PopIncomingMessage();
    std::uint32_t QueuedMessageCount() const;

    // IMessageSink {
    virtual bool CollectMessage(
        std::weak_ptr<CBryptPeer> const& wpBryptPeer,
        CMessageContext const& context,
        std::string_view buffer) override;
        
    virtual bool CollectMessage(
        std::weak_ptr<CBryptPeer> const& wpBryptPeer,
        CMessageContext const& context,
        Message::Buffer const& buffer) override;
    // }IMessageSink

private:
    bool CollectMessage(
        std::weak_ptr<CBryptPeer> const& wpBryptPeer,
        CApplicationMessage const& message);

    mutable std::shared_mutex m_incomingMutex;
    std::queue<AssociatedMessage> m_incoming;

};

//------------------------------------------------------------------------------------------------
