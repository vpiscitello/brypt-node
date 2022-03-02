//----------------------------------------------------------------------------------------------------------------------
// File: SinglePeerMediatorStub.cpp
// Description: 
//----------------------------------------------------------------------------------------------------------------------
#include "SinglePeerMediatorStub.hpp"
#include "BryptMessage/PlatformMessage.hpp"
#include "Components/Peer/Proxy.hpp"
#include "Components/Security/SecurityDefinitions.hpp"
#include "Interfaces/SecurityStrategy.hpp"
//----------------------------------------------------------------------------------------------------------------------
#include <cassert>
//----------------------------------------------------------------------------------------------------------------------

//----------------------------------------------------------------------------------------------------------------------
namespace {
namespace local {
//----------------------------------------------------------------------------------------------------------------------

class SecurityStrategyStub;

//----------------------------------------------------------------------------------------------------------------------
} // local namespace
} // namespace
//----------------------------------------------------------------------------------------------------------------------

class local::SecurityStrategyStub : public ISecurityStrategy
{
public:
    SecurityStrategyStub();

    virtual Security::Strategy GetStrategyType() const override;
    virtual Security::Role GetRoleType() const override;
    virtual Security::Context GetContextType() const override;
    virtual std::size_t GetSignatureSize() const override;

    virtual std::uint32_t GetSynchronizationStages() const override;
    virtual Security::SynchronizationStatus GetSynchronizationStatus() const override;
    virtual Security::SynchronizationResult PrepareSynchronization() override;
    virtual Security::SynchronizationResult Synchronize(Security::ReadableView) override;

    virtual Security::OptionalBuffer Encrypt(
        Security::ReadableView, std::uint64_t) const override;
    virtual Security::OptionalBuffer Decrypt(
        Security::ReadableView, std::uint64_t) const override;

    virtual std::int32_t Sign(Security::Buffer&) const override;
    virtual Security::VerificationStatus Verify(Security::ReadableView) const override;

private: 
    virtual std::int32_t Sign( Security::ReadableView, Security::Buffer&) const override;

    virtual Security::OptionalBuffer GenerateSignature(
        Security::ReadableView, Security::ReadableView) const override;
};

//----------------------------------------------------------------------------------------------------------------------

SinglePeerMediatorStub::SinglePeerMediatorStub(
    Node::SharedIdentifier const& spNodeIdentifier,
    IMessageSink* const pMessageSink)
    : m_spNodeIdentifier(spNodeIdentifier)
    , m_spPeer()
    , m_pMessageSink(pMessageSink)
{
}

//----------------------------------------------------------------------------------------------------------------------

void SinglePeerMediatorStub::RegisterObserver([[maybe_unused]] IPeerObserver* const observer) {}

//----------------------------------------------------------------------------------------------------------------------

void SinglePeerMediatorStub::UnpublishObserver([[maybe_unused]] IPeerObserver* const observer) {}

//----------------------------------------------------------------------------------------------------------------------

SinglePeerMediatorStub::OptionalRequest SinglePeerMediatorStub::DeclareResolvingPeer(
    [[maybe_unused]] Network::RemoteAddress const& address,
    [[maybe_unused]] Node::SharedIdentifier const& spIdentifier)
{
    auto const optHeartbeatRequest = Message::Platform::Parcel::GetBuilder()
        .MakeHeartbeatRequest()
        .SetSource(*m_spNodeIdentifier)
        .ValidatedBuild();
    assert(optHeartbeatRequest);

    return optHeartbeatRequest->GetPack();
}

//----------------------------------------------------------------------------------------------------------------------

void SinglePeerMediatorStub::RescindResolvingPeer([[maybe_unused]] Network::RemoteAddress const& address) {}

//----------------------------------------------------------------------------------------------------------------------

std::shared_ptr<Peer::Proxy> SinglePeerMediatorStub::LinkPeer(
    Node::Identifier const& identifier,
    [[maybe_unused]] Network::RemoteAddress const& address)
{
    if (m_spPeer) { return m_spPeer; }
    m_spPeer = Peer::Proxy::CreateInstance(identifier, std::weak_ptr<IMessageSink>{}, this);
    m_spPeer->AttachSecurityStrategy<InvokeContext::Test>(std::make_unique<local::SecurityStrategyStub>());
    m_spPeer->SetReceiver<InvokeContext::Test>(m_pMessageSink);
    return m_spPeer;
}

//----------------------------------------------------------------------------------------------------------------------

void SinglePeerMediatorStub::OnEndpointRegistered(
    std::shared_ptr<Peer::Proxy> const&, Network::Endpoint::Identifier, Network::RemoteAddress const&) {};

//----------------------------------------------------------------------------------------------------------------------

void SinglePeerMediatorStub::OnEndpointWithdrawn(
    std::shared_ptr<Peer::Proxy> const&, Network::Endpoint::Identifier, Network::RemoteAddress const&, WithdrawalCause)
{
}

//----------------------------------------------------------------------------------------------------------------------

std::shared_ptr<Peer::Proxy> SinglePeerMediatorStub::GetPeer() const { return m_spPeer; }

//----------------------------------------------------------------------------------------------------------------------

void SinglePeerMediatorStub::Reset() { m_spPeer.reset(); }

//----------------------------------------------------------------------------------------------------------------------

local::SecurityStrategyStub::SecurityStrategyStub() {}

//----------------------------------------------------------------------------------------------------------------------

Security::Strategy local::SecurityStrategyStub::GetStrategyType() const { return Security::Strategy::Invalid; }

//----------------------------------------------------------------------------------------------------------------------

Security::Role local::SecurityStrategyStub::GetRoleType() const { return Security::Role::Initiator; }

//----------------------------------------------------------------------------------------------------------------------

Security::Context local::SecurityStrategyStub::GetContextType() const { return Security::Context::Unique; }

//----------------------------------------------------------------------------------------------------------------------

std::size_t local::SecurityStrategyStub::GetSignatureSize() const { return 0; }

//----------------------------------------------------------------------------------------------------------------------

std::uint32_t local::SecurityStrategyStub::GetSynchronizationStages() const { return 0; }

//----------------------------------------------------------------------------------------------------------------------

Security::SynchronizationStatus local::SecurityStrategyStub::GetSynchronizationStatus() const
{
    return Security::SynchronizationStatus::Processing;
}

//----------------------------------------------------------------------------------------------------------------------

Security::SynchronizationResult local::SecurityStrategyStub::PrepareSynchronization()
{
    return { Security::SynchronizationStatus::Processing, {} };
}

//----------------------------------------------------------------------------------------------------------------------

Security::SynchronizationResult local::SecurityStrategyStub::Synchronize(Security::ReadableView)
{
    return { Security::SynchronizationStatus::Processing, {} };
}

//----------------------------------------------------------------------------------------------------------------------

Security::OptionalBuffer local::SecurityStrategyStub::Encrypt(
    Security::ReadableView buffer, [[maybe_unused]] std::uint64_t) const
{
    return Security::Buffer(buffer.begin(), buffer.end());
}

//----------------------------------------------------------------------------------------------------------------------

Security::OptionalBuffer local::SecurityStrategyStub::Decrypt(
    Security::ReadableView buffer, [[maybe_unused]] std::uint64_t) const
{
    return Security::Buffer(buffer.begin(), buffer.end());
}

//----------------------------------------------------------------------------------------------------------------------

std::int32_t local::SecurityStrategyStub::Sign([[maybe_unused]] Security::Buffer&) const { return 0; }

//----------------------------------------------------------------------------------------------------------------------

Security::VerificationStatus local::SecurityStrategyStub::Verify([[maybe_unused]] Security::ReadableView) const
{
    return Security::VerificationStatus::Success;
}

//----------------------------------------------------------------------------------------------------------------------

std::int32_t local::SecurityStrategyStub::Sign(
    [[maybe_unused]] Security::ReadableView, [[maybe_unused]] Security::Buffer&) const { return 0; }

//----------------------------------------------------------------------------------------------------------------------

Security::OptionalBuffer local::SecurityStrategyStub::GenerateSignature(
    [[maybe_unused]] Security::ReadableView, [[maybe_unused]] Security::ReadableView) const { return {}; }

//----------------------------------------------------------------------------------------------------------------------
