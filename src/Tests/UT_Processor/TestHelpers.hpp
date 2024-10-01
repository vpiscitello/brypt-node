//----------------------------------------------------------------------------------------------------------------------
#include "Components/Awaitable/Definitions.hpp"
#include "Components/Identifier/BryptIdentifier.hpp"
#include "Components/Message/ApplicationMessage.hpp"
#include "Components/Network/Address.hpp"
#include "Components/Network/EndpointIdentifier.hpp"
#include "Components/Network/Protocol.hpp"
#include "Components/Peer/Proxy.hpp"
#include "Interfaces/PeerCache.hpp"
#include "Interfaces/ConnectProtocol.hpp"
#include "Interfaces/ExchangeObserver.hpp"
//----------------------------------------------------------------------------------------------------------------------
#include <cstdint>
#include <ranges>
#include <string_view>
//----------------------------------------------------------------------------------------------------------------------

//----------------------------------------------------------------------------------------------------------------------
namespace Processor::Test {
//----------------------------------------------------------------------------------------------------------------------

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
} // Processor::Test namespace
//----------------------------------------------------------------------------------------------------------------------

class Processor::Test::ConnectProtocol : public IConnectProtocol
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

class Processor::Test::ExchangeObserver : public IExchangeObserver
{
public:
    ExchangeObserver() : m_optStatus(), m_upCipherPackage() { }

    // IExchangeObserver {
    virtual void OnExchangeClose(ExchangeStatus status) override { m_optStatus = status; }

    virtual void OnFulfilledStrategy(std::unique_ptr<Security::CipherPackage>&& upCipherPackage) override
    {
        m_upCipherPackage = std::move(upCipherPackage);
    }
    // } IExchangeObserver 

    bool Notified() const { return m_optStatus.has_value(); }
    std::optional<ExchangeStatus> GetExchangeStatus() const { return m_optStatus; }

    bool ExchangeSuccess() const
    {
        // The exchange was successful if we were notified of a success and acquired a synchronized
        // security strategy. 
        bool const success = (m_optStatus == ExchangeStatus::Success);
        return (success && m_upCipherPackage);
    }

private: 
    std::optional<ExchangeStatus> m_optStatus;
    std::unique_ptr<Security::CipherPackage> m_upCipherPackage;
};

//----------------------------------------------------------------------------------------------------------------------
