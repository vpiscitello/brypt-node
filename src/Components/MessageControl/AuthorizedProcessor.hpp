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

class BryptPeer;
class ApplicationMessage;
class NetworkMessage;
class MessageContext;

//----------------------------------------------------------------------------------------------------------------------

class AuthorizedProcessor : public IMessageSink {
public:
    explicit AuthorizedProcessor(BryptIdentifier::SharedContainer const& spBryptIdentifier);

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
    std::size_t QueuedMessageCount() const;

private:
    bool QueueMessage(
        std::weak_ptr<BryptPeer> const& wpBryptPeer,
        ApplicationMessage const& message);

    bool HandleMessage(
        std::weak_ptr<BryptPeer> const& wpBryptPeer,
    	NetworkMessage const& message);
        
    BryptIdentifier::SharedContainer m_spBryptIdentifier;
    
    mutable std::shared_mutex m_incomingMutex;
    std::queue<AssociatedMessage> m_incoming;
};

//----------------------------------------------------------------------------------------------------------------------
