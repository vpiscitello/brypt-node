//----------------------------------------------------------------------------------------------------------------------
#include "BryptIdentifier/BryptIdentifier.hpp"
#include "BryptMessage/ApplicationMessage.hpp"
#include "Components/Awaitable/Definitions.hpp"
#include "Components/Configuration/BootstrapService.hpp"
#include "Components/Network/Address.hpp"
#include "Components/Network/Endpoint.hpp"
#include "Components/Network/EndpointIdentifier.hpp"
#include "Components/Network/Protocol.hpp"
#include "Interfaces/PeerCache.hpp"
#include "Utilities/InvokeContext.hpp"
//----------------------------------------------------------------------------------------------------------------------
#include <cstdint>
#include <ranges>
#include <string_view>
//----------------------------------------------------------------------------------------------------------------------

//----------------------------------------------------------------------------------------------------------------------
namespace Route::Test {
//----------------------------------------------------------------------------------------------------------------------

class PeerCache;
class StandardEndpoint;

Message::Context GenerateMessageContext();

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
} // Route::Test namespace
//----------------------------------------------------------------------------------------------------------------------

class Route::Test::PeerCache : public IPeerCache
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

class Route::Test::StandardEndpoint : public Network::IEndpoint
{
public:
    explicit StandardEndpoint(Network::Endpoint::Properties const& properties)
        : IEndpoint(properties)
        , m_scheduled(0)
        , m_connected()
    {
    }

    std::uint32_t const& GetScheduled() const { return m_scheduled; }
    BootstrapService::BootstrapCache const& GetConnected() const { return m_connected; }

    // IEndpoint {
    [[nodiscard]] virtual Network::Protocol GetProtocol() const override { return Network::Protocol::Test; }
    [[nodiscard]] virtual std::string_view GetScheme() const override { return Network::TestScheme; }
    [[nodiscard]] virtual Network::BindingAddress GetBinding() const override { return m_binding; }

    virtual void Startup() override {}
	[[nodiscard]] virtual bool Shutdown() override { return true; }
    [[nodiscard]] virtual bool IsActive() const override { return true; }

    [[nodiscard]] virtual bool ScheduleBind(Network::BindingAddress const&) override { return true; }
    [[nodiscard]] virtual bool ScheduleConnect(Network::RemoteAddress const& address) override
    {
        return ScheduleConnect(Network::RemoteAddress{ address }, nullptr);
    }
    [[nodiscard]] virtual bool ScheduleConnect(Network::RemoteAddress&& address) override
    {
        return ScheduleConnect(std::move(address), nullptr);
    }
    [[nodiscard]] virtual bool ScheduleConnect(Network::RemoteAddress&& address, Node::SharedIdentifier const&) override
    {
        ++m_scheduled;
        m_connected.emplace(address);
        return true;
    }
    [[nodiscard]] virtual bool ScheduleDisconnect(Network::RemoteAddress const&) override { return false; }
    [[nodiscard]] virtual bool ScheduleDisconnect(Network::RemoteAddress&&) override { return false; }
	[[nodiscard]] virtual bool ScheduleSend(Node::Identifier const&, std::string&&) override { return true; }
    [[nodiscard]] virtual bool ScheduleSend(Node::Identifier const&, Message::ShareablePack const&) override { return true; }
    [[nodiscard]] virtual bool ScheduleSend(Node::Identifier const&, Network::MessageVariant&&) override { return true; }
    // } IEndpoint

private:
    std::uint32_t m_scheduled;
    BootstrapService::BootstrapCache m_connected;
};

//----------------------------------------------------------------------------------------------------------------------

inline Message::Context Route::Test::GenerateMessageContext()
{
    Message::Context context{ EndpointIdentifier, EndpointProtocol };

    context.BindEncryptionHandlers(
        [] (auto const& buffer, auto) -> Security::Encryptor::result_type 
            { return Security::Buffer(buffer.begin(), buffer.end()); },
        [] (auto const& buffer, auto) -> Security::Decryptor::result_type 
            { return Security::Buffer(buffer.begin(), buffer.end()); });

    context.BindSignatureHandlers(
        [] (auto&) -> Security::Signator::result_type  { return 0; },
        [] (auto const&) -> Security::Verifier::result_type { return Security::VerificationStatus::Success; },
        [] () -> Security::SignatureSizeGetter::result_type { return 0; });

    return context;
}

//----------------------------------------------------------------------------------------------------------------------
