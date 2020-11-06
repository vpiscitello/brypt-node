//------------------------------------------------------------------------------------------------
// File: AuthorizedProcessor.hpp
// Description:
//------------------------------------------------------------------------------------------------
#pragma once
//------------------------------------------------------------------------------------------------
#include "AssociatedMessage.hpp"
#include "../../BryptMessage/MessageTypes.hpp"
#include "../../Interfaces/MessageSink.hpp"
//------------------------------------------------------------------------------------------------
#include <cstdint>
#include <memory>
#include <optional>
#include <queue>
#include <shared_mutex>
//------------------------------------------------------------------------------------------------

class CBryptPeer;
class CApplicationMessage;
class CNetworkMessage;
class CMessageContext;

//------------------------------------------------------------------------------------------------

class CAuthorizedProcessor : public IMessageSink {
public:
    CAuthorizedProcessor();

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
    
    std::optional<AssociatedMessage> PopIncomingMessage();
    std::uint32_t QueuedMessageCount() const;

private:
    bool QueueMessage(
        std::weak_ptr<CBryptPeer> const& wpBryptPeer,
        CApplicationMessage const& message);

    bool HandleMessage(
        std::weak_ptr<CBryptPeer> const& wpBryptPeer,
    	CNetworkMessage const& message);
        
    mutable std::shared_mutex m_incomingMutex;
    std::queue<AssociatedMessage> m_incoming;

};

//------------------------------------------------------------------------------------------------
