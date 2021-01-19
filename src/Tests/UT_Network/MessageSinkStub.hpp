//------------------------------------------------------------------------------------------------
// File: MessageSinkStub.hpp
// Description: An IMessageSink stub implementation for collecting message received through 
// endpoint tests. 
//------------------------------------------------------------------------------------------------
#pragma once
//------------------------------------------------------------------------------------------------
#include "BryptIdentifier/IdentifierTypes.hpp"
#include "BryptMessage/MessageTypes.hpp"
#include "Components/MessageControl/AssociatedMessage.hpp"
#include "Interfaces/MessageSink.hpp"
//------------------------------------------------------------------------------------------------
#include <cstdint>
#include <memory>
#include <optional>
#include <queue>
#include <shared_mutex>
//------------------------------------------------------------------------------------------------

class BryptPeer;
class ApplicationMessage;
class MessageContext;

//------------------------------------------------------------------------------------------------

class MessageSinkStub : public IMessageSink {
public:
    explicit MessageSinkStub(BryptIdentifier::SharedContainer const& spBryptIdentifier);
    
    // IMessageSink {
    virtual bool CollectMessage(
        std::weak_ptr<BryptPeer> const& wpBryptPeer,
        MessageContext const& context,
        std::string_view buffer) override;
        
    virtual bool CollectMessage(
        std::weak_ptr<BryptPeer> const& wpBryptPeer,
        MessageContext const& context,
        std::span<std::uint8_t const> buffer) override;
    // }IMessageSink

    std::optional<AssociatedMessage> GetNextMessage();
    bool ReceviedHeartbeatRequest() const;
    bool ReceviedHeartbeatResponse() const;
    std::uint32_t InvalidMessageCount() const;
    
private:
    bool QueueMessage(
        std::weak_ptr<BryptPeer> const& wpBryptPeer,
        ApplicationMessage const& message);

    mutable std::shared_mutex m_mutex;
    
    BryptIdentifier::SharedContainer m_spBryptIdentifier;
    std::queue<AssociatedMessage> m_incoming;

    bool m_bReceivedHeartbeatRequest;
    bool m_bReceivedHeartbeatResponse;
    std::uint32_t m_invalidMessageCount;
};

//------------------------------------------------------------------------------------------------
