//----------------------------------------------------------------------------------------------------------------------
#include "Components/Core/ServiceProvider.hpp"
#include "Components/Event/Publisher.hpp"
#include "Components/Identifier/BryptIdentifier.hpp"
#include "Components/Message/MessageContext.hpp"
#include "Components/Network/ConnectionState.hpp"
#include "Components/Network/Address.hpp"
#include "Components/Peer/Proxy.hpp"
#include "Interfaces/ConnectProtocol.hpp"
#include "Interfaces/PeerCache.hpp"
#include "Interfaces/PeerObserver.hpp"
#include "Interfaces/ResolutionService.hpp"
//----------------------------------------------------------------------------------------------------------------------
#include <cstdint>
#include <memory>
#include <ranges>
#include <span>
#include <string_view>
//----------------------------------------------------------------------------------------------------------------------

//----------------------------------------------------------------------------------------------------------------------
namespace Peer::Test {
//----------------------------------------------------------------------------------------------------------------------

class ResolutionService;
class PeerCache;
class ConnectProtocol;
class MessageProcessor;
class Synchronizer;
class SynchronousObserver;
class AsynchronousObserver;

constexpr std::string_view NoticeRoute = "/notice";
constexpr std::string_view RequestRoute = "/request";
constexpr std::string_view ResponseRoute = "/response";

constexpr std::string_view const ApplicationPayload = "Application Payload";
constexpr std::string_view const HandshakePayload = "Handshake Request";
constexpr std::string_view const NoticePayload = "Notice Payload";
constexpr std::string_view const RequestPayload = "Request Payload";

constexpr Network::Endpoint::Identifier const EndpointIdentifier = 1;
constexpr Network::Protocol const EndpointProtocol = Network::Protocol::TCP;

Network::RemoteAddress const RemoteServerAddress{ Network::Protocol::TCP, "127.0.0.1:35216", true };
Network::RemoteAddress const RemoteClientAddress{ Network::Protocol::TCP, "127.0.0.1:35217", false };

constexpr Awaitable::TrackerKey TrackerKey = {
    0x01, 0x23, 0x45, 0x67, 0x89, 0xAB, 0xCD, 0xEF,
    0xEF, 0xCD, 0xAB, 0x89, 0x67, 0x45, 0x23, 0x01
};

//----------------------------------------------------------------------------------------------------------------------
} // Peer::Test namespace
//----------------------------------------------------------------------------------------------------------------------

class Peer::Test::ResolutionService : public IResolutionService
{
public:
    ResolutionService() = default;
    virtual void RegisterObserver(IPeerObserver* const) override { }
    virtual void UnpublishObserver(IPeerObserver* const) override { }
    virtual OptionalRequest DeclareResolvingPeer(Network::RemoteAddress const&, Node::SharedIdentifier const&) override { return {}; }
    virtual void RescindResolvingPeer(Network::RemoteAddress const&) override { }
    virtual std::shared_ptr<Peer::Proxy> LinkPeer(Node::Identifier const&, Network::RemoteAddress const&) override { return nullptr; }
    void OnEndpointRegistered(std::shared_ptr<Peer::Proxy> const&, Network::Endpoint::Identifier, Network::RemoteAddress const&) { }
    void OnEndpointWithdrawn(std::shared_ptr<Peer::Proxy> const&, Network::Endpoint::Identifier, Network::RemoteAddress const&, WithdrawalCause) { }
};

//----------------------------------------------------------------------------------------------------------------------

class Peer::Test::PeerCache : public IPeerCache
{
public:
    explicit PeerCache(std::size_t generate)
        : m_identifiers()
    {
        std::ranges::generate_n(std::back_insert_iterator(m_identifiers), generate, [] () mutable { 
            return std::make_shared<Node::Identifier const>(Node::GenerateIdentifier());
        });
    }

    explicit PeerCache(std::vector<Node::SharedIdentifier> const& identifiers)
        : m_identifiers(identifiers)
    {
    }

    virtual bool ForEach(IdentifierReadFunction const& callback, Filter) const override
    {
        std::ranges::for_each(m_identifiers, [&callback] (auto const& spNodeIdentifier) {
            callback(spNodeIdentifier);
        });
        return true;
    }

    [[nodiscard]] virtual std::size_t ActiveCount() const override { return m_identifiers.size(); }
    [[nodiscard]] virtual std::size_t InactiveCount() const  override { return 0; }
    [[nodiscard]] virtual std::size_t ObservedCount() const override { return m_identifiers.size(); }
    [[nodiscard]] virtual std::size_t ResolvingCount() const override { return 0; }

private: 
    std::vector<Node::SharedIdentifier> m_identifiers;
};

//----------------------------------------------------------------------------------------------------------------------

class Peer::Test::ConnectProtocol : public IConnectProtocol
{
public:
    ConnectProtocol()
        : m_count(0)
    {
    }

    virtual bool SendRequest(std::shared_ptr<Peer::Proxy> const&, Message::Context const&) const override
    {
        ++m_count;
        return true;
    }

    bool CalledOnce() const { return m_count == 1; }

private:
    mutable std::uint32_t m_count;
};

//----------------------------------------------------------------------------------------------------------------------

class Peer::Test::MessageProcessor : public IMessageSink
{
public:
    MessageProcessor() = default;

    virtual bool CollectMessage(Message::Context const&, std::string_view buffer) override
    {
        m_pack = std::string{ buffer.begin(), buffer.end() };
        return true;
    }

    virtual bool CollectMessage(Message::Context const&, std::span<std::uint8_t const>) override
    {
        return false;
    }
    
    std::string const& GetCollectedPack() const { return m_pack; }

private:
    std::string m_pack;
};

//----------------------------------------------------------------------------------------------------------------------

class Peer::Test::Synchronizer : public ISynchronizer
{
public:
    Synchronizer() = default;

    [[nodiscard]] virtual Security::ExchangeRole GetExchangeRole() const override { return Security::ExchangeRole::Initiator; }

    [[nodiscard]] virtual std::uint32_t GetStages() const override { return 0; }
    [[nodiscard]] virtual Security::SynchronizationStatus GetStatus() const override { return Security::SynchronizationStatus::Processing; }
    [[nodiscard]] virtual bool Synchronized() const override { return false; }

    [[nodiscard]] virtual Security::SynchronizationResult Initialize() override
    {
        return { Security::SynchronizationStatus::Processing, {} };
    }

    [[nodiscard]] virtual Security::SynchronizationResult Synchronize(Security::ReadableView buffer) override
    {
        return { Security::SynchronizationStatus::Processing, {} };
    }

    [[nodiscard]] virtual std::unique_ptr<Security::CipherPackage> Finalize() override
    {
        return nullptr;
    }
};

//----------------------------------------------------------------------------------------------------------------------

class Peer::Test::SynchronousObserver : public IPeerObserver
{
public:
    explicit SynchronousObserver(IResolutionService* const mediator)
        : m_mediator(mediator)
        , m_state(Network::Connection::State::Unknown)
    {
        m_mediator->RegisterObserver(this);
    }

    SynchronousObserver(SynchronousObserver&& other)
        : m_mediator(other.m_mediator)
        , m_state(other.m_state)
    {
        m_mediator->RegisterObserver(this);
    }

    ~SynchronousObserver()
    {
        m_mediator->UnpublishObserver(this);
    }

    void OnRemoteConnected(Network::Endpoint::Identifier, Network::RemoteAddress const&) override
    {
        m_state = Network::Connection::State::Connected;
    }

    void OnRemoteDisconnected(Network::Endpoint::Identifier, Network::RemoteAddress const&) override
    {
        m_state = Network::Connection::State::Disconnected;
    }

    Network::Connection::State GetConnectionState() const { return m_state; }

private:
    IResolutionService* m_mediator;
    Network::Connection::State m_state;
};

//----------------------------------------------------------------------------------------------------------------------

class Peer::Test::AsynchronousObserver
{
public:
    using EventRecord = std::vector<Event::Type>;
    using EventTracker = std::unordered_map<Node::Identifier, EventRecord, Node::IdentifierHasher>;

    AsynchronousObserver(Event::SharedPublisher const& spPublisher, Node::Identifier const& identifier)
        : m_spPublisher(spPublisher)
        , m_tracker()
    {
        using DisconnectCause = Event::Message<Event::Type::PeerDisconnected>::Cause;

        m_tracker.emplace(identifier, EventRecord{}); // Make an event record using the provided peer identifier. 

        // Subscribe to all events fired by an endpoint. Each listener should only record valid events. 
        spPublisher->Subscribe<Event::Type::PeerConnected>(
            [&tracker = m_tracker]
            (std::weak_ptr<Peer::Proxy> const& wpProxy, Network::RemoteAddress const& address)
            {
                if (auto const spProxy = wpProxy.lock(); spProxy && address.GetProtocol() != Network::Protocol::Invalid) {
                    if (auto const itr = tracker.find(*spProxy->GetIdentifier()); itr != tracker.end()) {
                        itr->second.emplace_back(Event::Type::PeerConnected);
                    }
                }
            });

        spPublisher->Subscribe<Event::Type::PeerDisconnected>(
            [&tracker = m_tracker] 
            (std::weak_ptr<Peer::Proxy> const& wpProxy, Network::RemoteAddress const& address, DisconnectCause cause)
            {
                if (cause != DisconnectCause::SessionClosure) { return; }
                if (auto const spProxy = wpProxy.lock(); spProxy && address.GetProtocol() != Network::Protocol::Invalid) {
                    if (auto const itr = tracker.find(*spProxy->GetIdentifier()); itr != tracker.end()) {
                        itr->second.emplace_back(Event::Type::PeerDisconnected);
                    }
                }
            });
    }

    [[nodiscard]] bool SubscribedToAllAdvertisedEvents() const
    {
        // We expect to be subscribed to all events advertised by an endpoint. A failure here is most likely caused
        // by this test fixture being outdated. 
        return m_spPublisher->ListenerCount() == m_spPublisher->AdvertisedCount();
    }

    [[nodiscard]] bool ReceivedExpectedEventSequence() const
    {
        if (m_spPublisher->Dispatch() == 0) { return false; } // We expect that events have been published. 

        // Count the number of peers that match the expected number and sequence of events (e.g. start becomes stop). 
        std::size_t const count = std::ranges::count_if(m_tracker | std::views::values,
            [] (EventRecord const& record) -> bool
            {
                if (record.size() != ExpectedEventCount) { return false; }
                if (record[0] != Event::Type::PeerConnected) { return false; }
                if (record[1] != Event::Type::PeerDisconnected) { return false; }
                return true;
            });

        // We expect that all endpoints tracked meet the event sequence expectations. 
        if (count != m_tracker.size()) { return false; }

        return true;
    }

private:
    static constexpr std::uint32_t ExpectedEventCount = 2; // The number of events each peer should fire. 
    Event::SharedPublisher m_spPublisher;
    EventTracker m_tracker;
};

//----------------------------------------------------------------------------------------------------------------------
