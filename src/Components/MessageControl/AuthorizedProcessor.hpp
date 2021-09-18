//----------------------------------------------------------------------------------------------------------------------
// File: AuthorizedProcessor.hpp
// Description:
//----------------------------------------------------------------------------------------------------------------------
#pragma once
//----------------------------------------------------------------------------------------------------------------------
#include "AssociatedMessage.hpp"
#include "BryptMessage/MessageTypes.hpp"
#include "Components/Handler/Handler.hpp"
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
namespace Scheduler { class Delegate; class Registrar; }

class ApplicationMessage;
class NetworkMessage;
class MessageContext;

//----------------------------------------------------------------------------------------------------------------------

class AuthorizedProcessor : public IMessageSink
{
public:
    AuthorizedProcessor(
        Node::SharedIdentifier const& spNodeIdentifier,
        Handler::Map const& handlers,
        std::shared_ptr<Scheduler::Registrar> const& spRegistrar);

    ~AuthorizedProcessor();

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
    std::size_t MessageCount() const;

private:
    [[nodiscard]] bool OnMessageCollected(
        std::weak_ptr<Peer::Proxy> const& wpPeerProxy, ApplicationMessage&& message);
    [[nodiscard]] bool OnMessageCollected(
        std::weak_ptr<Peer::Proxy> const& wpPeerProxy, NetworkMessage const& message);
        
    std::shared_ptr<Scheduler::Delegate> m_spDelegate;
    Node::SharedIdentifier m_spNodeIdentifier;
    
    mutable std::shared_mutex m_incomingMutex;
    std::queue<AssociatedMessage> m_incoming;
};

//----------------------------------------------------------------------------------------------------------------------
