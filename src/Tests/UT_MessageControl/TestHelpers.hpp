//----------------------------------------------------------------------------------------------------------------------
#include "BryptIdentifier/BryptIdentifier.hpp"
#include "BryptMessage/ApplicationMessage.hpp"
#include "Components/Awaitable/Definitions.hpp"
#include "Components/Network/Address.hpp"
#include "Components/Network/EndpointIdentifier.hpp"
#include "Components/Network/Protocol.hpp"
#include "Components/Peer/Proxy.hpp"
#include "Interfaces/PeerCache.hpp"
#include "Interfaces/ConnectProtocol.hpp"
#include "Interfaces/ExchangeObserver.hpp"
#include "Interfaces/SecurityStrategy.hpp"
//----------------------------------------------------------------------------------------------------------------------
#include <cstdint>
#include <ranges>
#include <string_view>
//----------------------------------------------------------------------------------------------------------------------

//----------------------------------------------------------------------------------------------------------------------
namespace MessageControl::Test {
//----------------------------------------------------------------------------------------------------------------------

class SecurityStrategy;
class ConnectProtocol;
class ExchangeObserver;

constexpr std::string_view Message = "Hello World!";

constexpr Network::Endpoint::Identifier EndpointIdentifier = 1;
constexpr Network::Protocol EndpointProtocol = Network::Protocol::Test;

auto const RemoteServerAddress = Network::RemoteAddress::CreateTestAddress<InvokeContext::Test>("*:35216", true);
auto const RemoteClientAddress = Network::RemoteAddress::CreateTestAddress<InvokeContext::Test>("*:35217", true);

constexpr Awaitable::TrackerKey TrackerKey = {
    0x01, 0x23, 0x45, 0x67, 0x89, 0xAB, 0xCD, 0xEF,
    0xEF, 0xCD, 0xAB, 0x89, 0x67, 0x45, 0x23, 0x01
};

//----------------------------------------------------------------------------------------------------------------------
} // MessageControl::Test namespace
//----------------------------------------------------------------------------------------------------------------------

class MessageControl::Test::SecurityStrategy : public ISecurityStrategy
{
public:
    SecurityStrategy() {}

    virtual Security::Strategy GetStrategyType() const override { return Security::Strategy::Invalid; }
    virtual Security::Role GetRoleType() const override { return Security::Role::Initiator; }
    virtual Security::Context GetContextType() const override { return Security::Context::Unique; }
    virtual std::size_t GetSignatureSize() const override { return 0; }

    virtual std::uint32_t GetSynchronizationStages() const override { return 0; }
    virtual Security::SynchronizationStatus GetSynchronizationStatus() const override
        { return Security::SynchronizationStatus::Processing; }
    virtual Security::SynchronizationResult PrepareSynchronization() override
        { return { Security::SynchronizationStatus::Processing, {} }; }
    virtual Security::SynchronizationResult Synchronize(Security::ReadableView) override
        { return { Security::SynchronizationStatus::Processing, {} }; }

    virtual Security::OptionalBuffer Encrypt(Security::ReadableView buffer, std::uint64_t) const override
        { return Security::Buffer(buffer.begin(), buffer.end()); }
    virtual Security::OptionalBuffer Decrypt(Security::ReadableView buffer, std::uint64_t) const override
        { return Security::Buffer(buffer.begin(), buffer.end()); }

    virtual std::int32_t Sign(Security::Buffer&) const override { return 0; }
    virtual Security::VerificationStatus Verify(Security::ReadableView) const override
        { return Security::VerificationStatus::Success; }

private: 
    virtual std::int32_t Sign(Security::ReadableView, Security::Buffer&) const override { return 0; }
    virtual Security::OptionalBuffer GenerateSignature(Security::ReadableView, Security::ReadableView) const override
        { return {}; }
};

//----------------------------------------------------------------------------------------------------------------------

class MessageControl::Test::ConnectProtocol : public IConnectProtocol
{
public:
    ConnectProtocol() : m_success(true), m_callers(0) { }

    // IConnectProtocol {
    virtual bool SendRequest(std::shared_ptr<Peer::Proxy> const& spProxy, Message::Context const&) const override
    {
        m_callers.emplace_back(spProxy->GetIdentifier<Node::Internal::Identifier>());
        return m_success;
    }
    // } IConnectProtocol 

    void FailSendRequests() { m_success = false; }

    [[nodiscard]] bool SentTo(Node::SharedIdentifier const& spNodeIdentifier) const
    {
        auto const itr = std::find(m_callers.begin(), m_callers.end(), *spNodeIdentifier);
        return itr != m_callers.end();
    }

    [[nodiscard]] std::size_t Called() const { return m_callers.size(); }

private:
    mutable bool m_success;
    mutable std::vector<Node::Internal::Identifier> m_callers;
};

//----------------------------------------------------------------------------------------------------------------------

class MessageControl::Test::ExchangeObserver : public IExchangeObserver
{
public:
    ExchangeObserver() : m_optStatus(), m_upSecurityStrategy() { }

    // IExchangeObserver {
    virtual void OnExchangeClose(ExchangeStatus status) override { m_optStatus = status; }

    virtual void OnFulfilledStrategy(std::unique_ptr<ISecurityStrategy>&& upStrategy) override
    {
        m_upSecurityStrategy = std::move(upStrategy);
    }
    // } IExchangeObserver 

    bool Notified() const { return m_optStatus.has_value(); }
    std::optional<ExchangeStatus> GetExchangeStatus() const { return m_optStatus; }

    bool ExchangeSuccess() const
    {
        // The exchange was successful if we were notified of a success and acquired a synchronized
        // security strategy. 
        bool const success = (m_optStatus == ExchangeStatus::Success);
        bool const ready = (
            m_upSecurityStrategy &&
            m_upSecurityStrategy->GetSynchronizationStatus() == Security::SynchronizationStatus::Ready);
        return (success && ready);
    }

private: 
    std::optional<ExchangeStatus> m_optStatus;
    std::unique_ptr<ISecurityStrategy> m_upSecurityStrategy;
};

//----------------------------------------------------------------------------------------------------------------------
