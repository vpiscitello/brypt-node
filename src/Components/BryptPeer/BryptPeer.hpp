//------------------------------------------------------------------------------------------------
// File: BryptPeer.hpp
// Description: 
//------------------------------------------------------------------------------------------------
#pragma once
//------------------------------------------------------------------------------------------------
#include "EndpointRegistration.hpp"
#include "PeerStatistics.hpp"
#include "../Endpoints/EndpointIdentifier.hpp"
#include "../Endpoints/MessageScheduler.hpp"
#include "../Endpoints/TechnologyType.hpp"
#include "../Security/SecurityState.hpp"
#include "../../BryptIdentifier/IdentifierTypes.hpp"
#include "../../Interfaces/MessageSink.hpp"
#include "../../BryptMessage/MessageTypes.hpp"
//------------------------------------------------------------------------------------------------
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <string_view>
#include <unordered_map>
//------------------------------------------------------------------------------------------------

class CMessageContext;
class CSecurityMediator;
class IPeerMediator;

//------------------------------------------------------------------------------------------------

class CBryptPeer : public std::enable_shared_from_this<CBryptPeer>
{
public:
    CBryptPeer(
        BryptIdentifier::CContainer const& identifier,
        IPeerMediator* const pPeerMediator = nullptr);

    ~CBryptPeer();

    BryptIdentifier::SharedContainer GetBryptIdentifier() const;
    BryptIdentifier::Internal::Type GetInternalIdentifier() const;

    // Statistic Methods {
    std::uint32_t GetSentCount() const;
    std::uint32_t GetReceivedCount() const;
    // } Statistic Methods

    // Message Receiving Methods {
    void SetReceiver(IMessageSink* const pMessageSink);
    bool ScheduleReceive(CMessageContext const& context, std::string_view const& buffer);
    bool ScheduleReceive(CMessageContext const& context, Message::Buffer const& buffer);
    // } Message Receiving Methods

    // Message Sending Methods {
    bool ScheduleSend(
        CMessageContext const& context, std::string_view const& message) const;
    // } Message Sending Methods

    // Endpoint Association Methods {
    void RegisterEndpoint(CEndpointRegistration const& registration);
    void RegisterEndpoint(
        Endpoints::EndpointIdType identifier,
        Endpoints::TechnologyType technology,
        MessageScheduler const& scheduler = {},
        std::string_view uri = {});

    void WithdrawEndpoint(
        Endpoints::EndpointIdType identifier, Endpoints::TechnologyType technology);

    bool IsActive() const;
    bool IsEndpointRegistered(Endpoints::EndpointIdType identifier) const;
    std::optional<std::string> GetRegisteredEntry(Endpoints::EndpointIdType identifier) const;
    std::uint32_t RegisteredEndpointCount() const;
    // } Endpoint Association Methods

    // Security Methods {
    void AttachSecurityMediator(std::unique_ptr<CSecurityMediator>&& upSecurityMediator);
    Security::State GetSecurityState() const;
    bool IsFlagged() const;
    bool IsAuthorized() const;
    // } Security Methods
    
private:
    using RegisteredEndpoints = std::unordered_map<
        Endpoints::EndpointIdType, CEndpointRegistration>;

    IPeerMediator* const m_pPeerMediator;

    mutable std::recursive_mutex m_dataMutex;
    BryptIdentifier::SharedContainer m_spBryptIdentifier;
    mutable CPeerStatistics m_statistics;

    mutable std::recursive_mutex m_mediatorMutex;
    std::unique_ptr<CSecurityMediator> m_upSecurityMediator;

    mutable std::recursive_mutex m_endpointsMutex;
    RegisteredEndpoints m_endpoints;

    mutable std::recursive_mutex m_receiverMutex;
    IMessageSink* m_pMessageSink;
};

//------------------------------------------------------------------------------------------------
