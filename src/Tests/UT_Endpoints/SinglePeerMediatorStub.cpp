//------------------------------------------------------------------------------------------------
// File: SinglePeerMediatorStub.cpp
// Description: 
//------------------------------------------------------------------------------------------------
#include "SinglePeerMediatorStub.hpp"
#include "../../BryptMessage/NetworkMessage.hpp"
#include "../../Components/BryptPeer/BryptPeer.hpp"
#include "../../Components/Security/SecurityMediator.hpp"
#include "../../Components/Security/SecurityDefinitions.hpp"
#include "../../Interfaces/SecurityStrategy.hpp"
//------------------------------------------------------------------------------------------------
#include <cassert>
//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
namespace {
namespace local {
//------------------------------------------------------------------------------------------------

class CSecurityStrategyStub;

//------------------------------------------------------------------------------------------------
} // local namespace
} // namespace
//------------------------------------------------------------------------------------------------

class local::CSecurityStrategyStub : public ISecurityStrategy
{
public:
    CSecurityStrategyStub();

    virtual Security::Strategy GetStrategyType() const override;
    virtual Security::Role GetRoleType() const override;
    virtual Security::Context GetContextType() const override;
    virtual std::uint32_t GetSignatureSize() const override;

    virtual std::uint32_t GetSynchronizationStages() const override;
    virtual Security::SynchronizationStatus GetSynchronizationStatus() const override;
    virtual Security::SynchronizationResult PrepareSynchronization() override;
    virtual Security::SynchronizationResult Synchronize(Security::Buffer const&) override;

    virtual Security::OptionalBuffer Encrypt(
        Security::Buffer const&, std::uint32_t, std::uint64_t) const override;
    virtual Security::OptionalBuffer Decrypt(
        Security::Buffer const&, std::uint32_t, std::uint64_t) const override;

    virtual std::int32_t Sign(Security::Buffer&) const override;
    virtual Security::VerificationStatus Verify(Security::Buffer const&) const override;

private: 
    virtual std::int32_t Sign(
        Security::Buffer const&, Security::Buffer&) const override;

    virtual Security::OptionalBuffer GenerateSignature(
        std::uint8_t const*,
        std::uint32_t,
        std::uint8_t const*,
        std::uint32_t) const override;
};

//------------------------------------------------------------------------------------------------

CSinglePeerMediatorStub::CSinglePeerMediatorStub(
    BryptIdentifier::SharedContainer const& spBryptIdentifier,
    IMessageSink* const pMessageSink)
    : m_spBryptIdentifier(spBryptIdentifier)
    , m_spBryptPeer()
    , m_pMessageSink(pMessageSink)
{
}

//------------------------------------------------------------------------------------------------

void CSinglePeerMediatorStub::RegisterObserver([[maybe_unused]] IPeerObserver* const observer)
{
}

//------------------------------------------------------------------------------------------------

void CSinglePeerMediatorStub::UnpublishObserver([[maybe_unused]] IPeerObserver* const observer)
{
}

//------------------------------------------------------------------------------------------------

CSinglePeerMediatorStub::OptionalRequest CSinglePeerMediatorStub::DeclareResolvingPeer(
    [[maybe_unused]] std::string_view uri)
{
    auto const optHeartbeatRequest = CNetworkMessage::Builder()
        .MakeHeartbeatRequest()
        .SetSource(*m_spBryptIdentifier)
        .ValidatedBuild();
    assert(optHeartbeatRequest);

    return optHeartbeatRequest->GetPack();
}

//------------------------------------------------------------------------------------------------

CSinglePeerMediatorStub::OptionalRequest CSinglePeerMediatorStub::DeclareResolvingPeer(
    BryptIdentifier::SharedContainer const& spIdentifier)
{
    if (!spIdentifier) {
        return {};
    }

    auto const optHeartbeatRequest = CNetworkMessage::Builder()
        .SetSource(*m_spBryptIdentifier)
        .SetDestination(*spIdentifier)
        .MakeHeartbeatRequest()
        .ValidatedBuild();
    assert(optHeartbeatRequest);

    return optHeartbeatRequest->GetPack();
}

//------------------------------------------------------------------------------------------------

std::shared_ptr<CBryptPeer> CSinglePeerMediatorStub::LinkPeer(
    BryptIdentifier::CContainer const& identifier,
    [[maybe_unused]] std::string_view uri)
{
    m_spBryptPeer = std::make_shared<CBryptPeer>(identifier, this);

    auto upSecurityStrategy = std::make_unique<local::CSecurityStrategyStub>();
    auto upSecurityMediator = std::make_unique<CSecurityMediator>(
            m_spBryptIdentifier, std::move(upSecurityStrategy));

    m_spBryptPeer->AttachSecurityMediator(std::move(upSecurityMediator));

    m_spBryptPeer->SetReceiver(m_pMessageSink);

    return m_spBryptPeer;
}

//------------------------------------------------------------------------------------------------

void CSinglePeerMediatorStub::DispatchPeerStateChange(
    [[maybe_unused]] std::weak_ptr<CBryptPeer> const& wpBryptPeer,
    [[maybe_unused]] Endpoints::EndpointIdType identifier,
    [[maybe_unused]] Endpoints::TechnologyType technology,
    [[maybe_unused]] ConnectionState change)
{
}

//------------------------------------------------------------------------------------------------

std::shared_ptr<CBryptPeer> CSinglePeerMediatorStub::GetPeer() const
{
    return m_spBryptPeer;
}

//------------------------------------------------------------------------------------------------


local::CSecurityStrategyStub::CSecurityStrategyStub()
{
}

//------------------------------------------------------------------------------------------------

Security::Strategy local::CSecurityStrategyStub::GetStrategyType() const
{
    return Security::Strategy::Invalid;
}

//------------------------------------------------------------------------------------------------

Security::Role local::CSecurityStrategyStub::GetRoleType() const
{
    return Security::Role::Initiator;
}

//------------------------------------------------------------------------------------------------

Security::Context local::CSecurityStrategyStub::GetContextType() const
{
    return Security::Context::Unique;
}

//------------------------------------------------------------------------------------------------

std::uint32_t local::CSecurityStrategyStub::GetSignatureSize() const
{
    return 0;
}

//------------------------------------------------------------------------------------------------

std::uint32_t local::CSecurityStrategyStub::GetSynchronizationStages() const
{
    return 0;
}

//------------------------------------------------------------------------------------------------

Security::SynchronizationStatus local::CSecurityStrategyStub::GetSynchronizationStatus() const
{
    return Security::SynchronizationStatus::Processing;
}

//------------------------------------------------------------------------------------------------

Security::SynchronizationResult local::CSecurityStrategyStub::PrepareSynchronization()
{
    return { Security::SynchronizationStatus::Processing, {} };
}

//------------------------------------------------------------------------------------------------

Security::SynchronizationResult local::CSecurityStrategyStub::Synchronize(Security::Buffer const&)
{
    return { Security::SynchronizationStatus::Processing, {} };
}

//------------------------------------------------------------------------------------------------

Security::OptionalBuffer local::CSecurityStrategyStub::Encrypt(
    Security::Buffer const& buffer,
    [[maybe_unused]] std::uint32_t,
    [[maybe_unused]] std::uint64_t) const
{
    return buffer;
}

//------------------------------------------------------------------------------------------------

Security::OptionalBuffer local::CSecurityStrategyStub::Decrypt(
    Security::Buffer const& buffer,
    [[maybe_unused]] std::uint32_t,
    [[maybe_unused]] std::uint64_t) const
{
    return buffer;
}


//------------------------------------------------------------------------------------------------

std::int32_t local::CSecurityStrategyStub::Sign(
    [[maybe_unused]] Security::Buffer&) const
{
    return 0;
}

//------------------------------------------------------------------------------------------------

Security::VerificationStatus local::CSecurityStrategyStub::Verify(
    [[maybe_unused]] Security::Buffer const&) const
{
    return Security::VerificationStatus::Success;
}

//------------------------------------------------------------------------------------------------

std::int32_t local::CSecurityStrategyStub::Sign(
    [[maybe_unused]] Security::Buffer const&, [[maybe_unused]] Security::Buffer&) const
{
    return 0;
}

//------------------------------------------------------------------------------------------------

Security::OptionalBuffer local::CSecurityStrategyStub::GenerateSignature(
    [[maybe_unused]] std::uint8_t const*,
    [[maybe_unused]] std::uint32_t,
    [[maybe_unused]] std::uint8_t const*,
    [[maybe_unused]] std::uint32_t) const
{
    return {};
}

//------------------------------------------------------------------------------------------------
