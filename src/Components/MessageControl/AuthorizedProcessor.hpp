//----------------------------------------------------------------------------------------------------------------------
// File: AuthorizedProcessor.hpp
// Description:
//----------------------------------------------------------------------------------------------------------------------
#pragma once
//----------------------------------------------------------------------------------------------------------------------
#include "AssociatedMessage.hpp"
#include "BryptMessage/MessageTypes.hpp"
#include "Interfaces/MessageSink.hpp"
//----------------------------------------------------------------------------------------------------------------------
#include <cstdint>
#include <memory>
#include <optional>
#include <queue>
#include <shared_mutex>
#include <span>
#include <string_view>
//----------------------------------------------------------------------------------------------------------------------

namespace Peer { class Proxy; }

class ApplicationMessage;
class NetworkMessage;
class MessageContext;

//----------------------------------------------------------------------------------------------------------------------

class AuthorizedProcessor : public IMessageSink {
public:
    explicit AuthorizedProcessor(Node::SharedIdentifier const& spNodeIdentifier);

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
    std::size_t QueuedMessageCount() const;

private:
    [[nodiscard]] bool QueueMessage(
        std::weak_ptr<Peer::Proxy> const& wpPeerProxy, ApplicationMessage&& message);
    [[nodiscard]] bool HandleMessage(
        std::weak_ptr<Peer::Proxy> const& wpPeerProxy, NetworkMessage const& message);
        
    Node::SharedIdentifier m_spNodeIdentifier;
    
    mutable std::shared_mutex m_incomingMutex;
    std::queue<AssociatedMessage> m_incoming;
};

//----------------------------------------------------------------------------------------------------------------------
