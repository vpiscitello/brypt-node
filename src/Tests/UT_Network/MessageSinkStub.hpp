//----------------------------------------------------------------------------------------------------------------------
// File: MessageSinkStub.hpp
// Description: An IMessageSink stub implementation for collecting message received through 
// endpoint tests. 
//----------------------------------------------------------------------------------------------------------------------
#pragma once
//----------------------------------------------------------------------------------------------------------------------
#include "BryptIdentifier/IdentifierTypes.hpp"
#include "BryptMessage/MessageTypes.hpp"
#include "Components/MessageControl/AssociatedMessage.hpp"
#include "Interfaces/MessageSink.hpp"
//----------------------------------------------------------------------------------------------------------------------
#include <cstdint>
#include <memory>
#include <optional>
#include <queue>
#include <shared_mutex>
//----------------------------------------------------------------------------------------------------------------------

namespace Peer { class Proxy; }

class ApplicationMessage;
class MessageContext;

//----------------------------------------------------------------------------------------------------------------------

class MessageSinkStub : public IMessageSink {
public:
    explicit MessageSinkStub(Node::SharedIdentifier const& spNodeIdentifier);
    
    // IMessageSink {
    virtual bool CollectMessage(
        std::weak_ptr<Peer::Proxy> const& wpPeerProxy,
        MessageContext const& context,
        std::string_view buffer) override;
        
    virtual bool CollectMessage(
        std::weak_ptr<Peer::Proxy> const& wpPeerProxy,
        MessageContext const& context,
        std::span<std::uint8_t const> buffer) override;
    // }IMessageSink

    std::optional<AssociatedMessage> GetNextMessage();
    bool ReceviedHeartbeatRequest() const;
    bool ReceviedHeartbeatResponse() const;
    std::uint32_t InvalidMessageCount() const;
    void Reset();
    
private:
    bool QueueMessage(std::weak_ptr<Peer::Proxy> const& wpPeerProxy, ApplicationMessage const& message);

    mutable std::shared_mutex m_mutex;
    
    Node::SharedIdentifier m_spNodeIdentifier;
    std::queue<AssociatedMessage> m_incoming;

    bool m_bReceivedHeartbeatRequest;
    bool m_bReceivedHeartbeatResponse;
    std::uint32_t m_invalidMessageCount;
};

//----------------------------------------------------------------------------------------------------------------------
