//----------------------------------------------------------------------------------------------------------------------
#include "BryptIdentifier/BryptIdentifier.hpp"
#include "BryptIdentifier/IdentifierTypes.hpp"
#include "BryptMessage/MessageContext.hpp"
#include "BryptMessage/MessageTypes.hpp"
#include "BryptMessage/MessageUtils.hpp"
#include "BryptMessage/PlatformMessage.hpp"
#include "BryptNode/ServiceProvider.hpp"
#include "Components/Configuration/Options.hpp"
#include "Components/Event/Publisher.hpp"
#include "Components/MessageControl/AssociatedMessage.hpp"
#include "Components/Network/Address.hpp"
#include "Components/Network/ConnectionState.hpp"
#include "Components/Network/EndpointIdentifier.hpp"
#include "Components/Network/Protocol.hpp"
#include "Components/Peer/Proxy.hpp"
#include "Interfaces/ConnectProtocol.hpp"
#include "Interfaces/MessageSink.hpp"
#include "Interfaces/PeerCache.hpp"
#include "Interfaces/PeerMediator.hpp"
#include "Interfaces/PeerObserver.hpp"
//----------------------------------------------------------------------------------------------------------------------
#include <cstdint>
#include <memory>
#include <optional>
#include <ranges>
#include <span>
#include <shared_mutex>
#include <string_view>
#include <queue>
//----------------------------------------------------------------------------------------------------------------------

//----------------------------------------------------------------------------------------------------------------------
namespace Network::Test {
//----------------------------------------------------------------------------------------------------------------------

class MessageProcessor;
class SecurityStrategy;
class SinglePeerMediator;

constexpr auto RuntimeOptions = Configuration::Options::Runtime
{ 
    .context = RuntimeContext::Foreground,
    .verbosity = spdlog::level::debug,
    .useInteractiveConsole = false,
    .useBootstraps = true,
    .useFilepathDeduction = false
};

//----------------------------------------------------------------------------------------------------------------------
} // Network::Test namespace
//----------------------------------------------------------------------------------------------------------------------

class Network::Test::MessageProcessor : public IMessageSink
{
public:
    explicit MessageProcessor(Node::SharedIdentifier const& spNodeIdentifier)
    	: m_mutex()
	    , m_spNodeIdentifier(spNodeIdentifier)
        , m_incoming()
        , m_bReceivedHeartbeatRequest(false)
        , m_bReceivedHeartbeatResponse(false)
        , m_invalidMessageCount(0)
    {
        assert(m_spNodeIdentifier);
    }
    
    // IMessageSink {
    virtual bool CollectMessage(
        std::weak_ptr<Peer::Proxy> const& wpPeerProxy,
        Message::Context const& context,
        std::string_view buffer) override
    {
        Message::Buffer const decoded = Z85::Decode(buffer); // Decode the buffer, it is expected to be Z85 encoded.
        return CollectMessage(wpPeerProxy, context, decoded); // Forward the message to the decoded buffer method. 
    }
        
    virtual bool CollectMessage(
        std::weak_ptr<Peer::Proxy> const& wpPeerProxy,
        Message::Context const& context,
        std::span<std::uint8_t const> buffer) override
    {
        auto const optProtocol = Message::PeekProtocol(buffer); // Peek the protocol in the packed buffer. 
        if (!optProtocol) { return false; }

        // Handle the message based on the message protocol indicated by the message.
        switch (*optProtocol) {
            // In the case of the application protocol, build an application message and add it too 
            // the queue if it is valid. 
            case Message::Protocol::Application: {
                // Build the application message.
                auto const optMessage = Message::Application::Parcel::GetBuilder()
                    .SetContext(context)
                    .FromDecodedPack(buffer)
                    .ValidatedBuild();

                // If the message is invalid increase the invalid count and return an error.
                if (!optMessage) { ++m_invalidMessageCount; return false; }

                return QueueMessage(wpPeerProxy, *optMessage);
            }
            // In the case of the network protocol, build a network message and process the message.
            case Message::Protocol::Platform: {
                auto const optRequest = Message::Platform::Parcel::GetBuilder()
                    .FromDecodedPack(buffer)
                    .ValidatedBuild();

                // If the message is invalid, increase the invalid count and return an error.
                if (!optRequest) { ++m_invalidMessageCount; return false; }

                // Process the message dependent on the network message type.
                switch (optRequest->GetType()) {
                    // In the case of a heartbeat request, build a heartbeat response and send it to the peer.
                    case Message::Platform::ParcelType::HeartbeatRequest: {
                        // Indicate we have received a heartbeat request for any tests.
                        m_bReceivedHeartbeatRequest = true;	

                        auto const optResponse = Message::Platform::Parcel::GetBuilder()
                            .MakeHeartbeatResponse()
                            .SetSource(*m_spNodeIdentifier)
                            .SetDestination(optRequest->GetSource())
                            .ValidatedBuild();
                        assert(optResponse);

                        if (auto const spPeerProxy = wpPeerProxy.lock(); spPeerProxy) {
                            return spPeerProxy->ScheduleSend(context.GetEndpointIdentifier(), optResponse->GetPack());
                        }
                        ++m_invalidMessageCount;
                    } return false;
                    case Message::Platform::ParcelType::HeartbeatResponse: {
                        m_bReceivedHeartbeatResponse = true;	
                    } return true;
                    // All other network messages are unexpected.
                    default: ++m_invalidMessageCount; return false;
                }
            }
            // All other message protocols are unexpected.
            case Message::Protocol::Invalid:
            default: ++m_invalidMessageCount; return false;
        }
    }
    // }IMessageSink

    std::optional<AssociatedMessage> GetNextMessage()
    {
        std::scoped_lock lock{ m_mutex };
        if (m_incoming.empty()) { return {}; }
        auto const message = std::move(m_incoming.front());
        m_incoming.pop();
        return message;
    }

    bool ReceviedHeartbeatRequest() const
    {
        std::scoped_lock lock{ m_mutex };
        return m_bReceivedHeartbeatRequest;
    }

    bool ReceviedHeartbeatResponse() const
    {
        std::scoped_lock lock{ m_mutex };
        return m_bReceivedHeartbeatResponse;
    }

    std::uint32_t InvalidMessageCount() const
    {
        std::scoped_lock lock{ m_mutex };
        return m_invalidMessageCount;
    }

    void Reset()
    {
        std::scoped_lock lock{ m_mutex };
        m_bReceivedHeartbeatRequest = false;
        m_bReceivedHeartbeatResponse = false;
    }

private:
    bool QueueMessage(std::weak_ptr<Peer::Proxy> const& wpPeerProxy, Message::Application::Parcel const& message)
    {
        std::scoped_lock lock{ m_mutex };
        m_incoming.emplace(AssociatedMessage{ wpPeerProxy, message });
        return true;
    }

    mutable std::shared_mutex m_mutex;
    
    Node::SharedIdentifier m_spNodeIdentifier;
    std::queue<AssociatedMessage> m_incoming;

    bool m_bReceivedHeartbeatRequest;
    bool m_bReceivedHeartbeatResponse;
    std::uint32_t m_invalidMessageCount;
};

//----------------------------------------------------------------------------------------------------------------------

class Network::Test::SecurityStrategy : public ISecurityStrategy
{
public:
    SecurityStrategy() = default;

    virtual Security::Strategy GetStrategyType() const override { return Security::Strategy::Invalid; }
    virtual Security::Role GetRoleType() const override { return Security::Role::Initiator; }
    virtual Security::Context GetContextType() const override { return Security::Context::Unique; }
    virtual std::size_t GetSignatureSize() const override { return 0; }

    virtual std::uint32_t GetSynchronizationStages() const override { return 0; }
    virtual Security::SynchronizationStatus GetSynchronizationStatus() const override { return Security::SynchronizationStatus::Processing; }
    virtual Security::SynchronizationResult PrepareSynchronization() override { return { Security::SynchronizationStatus::Processing, {} }; }
    virtual Security::SynchronizationResult Synchronize(Security::ReadableView) override { return { Security::SynchronizationStatus::Processing, {} }; }

    virtual Security::OptionalBuffer Encrypt(Security::ReadableView buffer, std::uint64_t) const override
    {
        return Security::Buffer{ buffer.begin(), buffer.end() };
    }

    virtual Security::OptionalBuffer Decrypt(Security::ReadableView buffer, std::uint64_t) const override
    {
        return Security::Buffer{ buffer.begin(), buffer.end() };
    }

    virtual std::int32_t Sign(Security::Buffer&) const override { return 0; }
    virtual Security::VerificationStatus Verify(Security::ReadableView) const override { return Security::VerificationStatus::Success; }

private: 
    virtual std::int32_t Sign(Security::ReadableView, Security::Buffer&) const override { return 0; }
    virtual Security::OptionalBuffer GenerateSignature(Security::ReadableView, Security::ReadableView) const override { return {}; }
};

//----------------------------------------------------------------------------------------------------------------------

class Network::Test::SinglePeerMediator : public IPeerMediator
{
public:
    SinglePeerMediator(
        Node::SharedIdentifier const& spNodeIdentifier,
        IMessageSink* const pMessageSink,
        std::shared_ptr<Node::ServiceProvider> spServiceProvider)
        : m_spNodeIdentifier(spNodeIdentifier)
        , m_spPeer()
        , m_pMessageSink(pMessageSink)
        , m_wpServiceProvider(spServiceProvider)
    {
    }

    // IPeerMediator {
    virtual void RegisterObserver(IPeerObserver* const) override { }
    virtual void UnpublishObserver(IPeerObserver* const) override { }

    virtual OptionalRequest DeclareResolvingPeer(Network::RemoteAddress const&, Node::SharedIdentifier const&) override
    {
        auto const optHeartbeatRequest = Message::Platform::Parcel::GetBuilder()
            .MakeHeartbeatRequest()
            .SetSource(*m_spNodeIdentifier)
            .ValidatedBuild();
        assert(optHeartbeatRequest);

        return optHeartbeatRequest->GetPack();
    }

    virtual void RescindResolvingPeer(Network::RemoteAddress const&) override { }

    virtual std::shared_ptr<Peer::Proxy> LinkPeer(Node::Identifier const& identifier, Network::RemoteAddress const&) override
    {
        if (!m_spPeer) { 
            m_spPeer = Peer::Proxy::CreateInstance(identifier, m_wpServiceProvider.lock());
        }
        m_spPeer->AttachSecurityStrategy<InvokeContext::Test>(std::make_unique<SecurityStrategy>());
        m_spPeer->SetReceiver<InvokeContext::Test>(m_pMessageSink);
        return m_spPeer;
    }

    virtual void OnEndpointRegistered(
        std::shared_ptr<Peer::Proxy> const&, Network::Endpoint::Identifier,  Network::RemoteAddress const&) override { }
    virtual void OnEndpointWithdrawn(
        std::shared_ptr<Peer::Proxy> const&, Network::Endpoint::Identifier, Network::RemoteAddress const&, WithdrawalCause) override { }
    // } IPeerMediator

    std::shared_ptr<Peer::Proxy> GetPeer() const { return m_spPeer; }
    void Reset() { m_spPeer.reset(); }

private:
    Node::SharedIdentifier m_spNodeIdentifier;
    std::shared_ptr<Peer::Proxy> m_spPeer;
    IMessageSink* const m_pMessageSink;
    std::weak_ptr<Node::ServiceProvider> m_wpServiceProvider;
};

//----------------------------------------------------------------------------------------------------------------------
