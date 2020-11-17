//------------------------------------------------------------------------------------------------
// File: MessageSinkStub.hpp
// Description: An IMessageSink stub implementation for collecting message received through 
// endpoint tests. 
//------------------------------------------------------------------------------------------------
#pragma once
//------------------------------------------------------------------------------------------------
#include "../../BryptIdentifier/IdentifierTypes.hpp"
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
    explicit CMessageSinkStub(BryptIdentifier::SharedContainer const& spBryptIdentifier);
    
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

    std::optional<AssociatedMessage> GetNextMessage();
    bool ReceviedHeartbeatRequest() const;
    bool ReceviedHeartbeatResponse() const;
    std::uint32_t InvalidMessageCount() const;
    
private:
    bool QueueMessage(
        std::weak_ptr<CBryptPeer> const& wpBryptPeer,
        CApplicationMessage const& message);

    mutable std::shared_mutex m_mutex;
    
    BryptIdentifier::SharedContainer m_spBryptIdentifier;
    std::queue<AssociatedMessage> m_incoming;

    bool m_bReceivedHeartbeatRequest;
    bool m_bReceivedHeartbeatResponse;
    std::uint32_t m_invalidMessageCount;

};

//------------------------------------------------------------------------------------------------
