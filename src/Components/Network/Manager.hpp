//----------------------------------------------------------------------------------------------------------------------
// File: Manager.hpp
// Description: 
//----------------------------------------------------------------------------------------------------------------------
#pragma once
//----------------------------------------------------------------------------------------------------------------------
#include "Address.hpp"
#include "EndpointIdentifier.hpp"
#include "EndpointTypes.hpp"
#include "Protocol.hpp"
#include "BryptNode/RuntimeContext.hpp"
#include "Components/Configuration/Options.hpp"
#include "Components/Configuration/BootstrapService.hpp"
#include "Components/Event/Events.hpp"
#include "Components/Event/SharedPublisher.hpp"
#include "Components/Peer/Manager.hpp"
#include "Interfaces/EndpointMediator.hpp"
#include "Interfaces/MessageSink.hpp"
#include "Interfaces/PeerMediator.hpp"
#include "Interfaces/PeerObserver.hpp"
#include "Utilities/NodeUtils.hpp"
//----------------------------------------------------------------------------------------------------------------------
#include <atomic>
#include <memory>
#include <shared_mutex>
#include <unordered_map>
#include <utility>
#include <vector>
//----------------------------------------------------------------------------------------------------------------------

//----------------------------------------------------------------------------------------------------------------------
namespace Network {
//----------------------------------------------------------------------------------------------------------------------

class IEndpoint;
class Manager;

//----------------------------------------------------------------------------------------------------------------------
}
//----------------------------------------------------------------------------------------------------------------------

class Network::Manager final : public IEndpointMediator
{
public:
    using SharedEndpoint = std::shared_ptr<IEndpoint>;

    Manager(
        Configuration::Options::Endpoints const& endpoints,
        Event::SharedPublisher const& spEventPublisher,
        IPeerMediator* const pPeerMediator,
        IBootstrapCache const* const pBootstrapCache,
        RuntimeContext context);

    Manager(Manager const& other) = delete;
    Manager& operator=(Manager const& other) = delete;

    ~Manager();

    // IEndpointMediator {
    [[nodiscard]] virtual bool IsRegisteredAddress(Address const& address) const override;
    virtual void UpdateBinding(Endpoint::Identifier identifier, BindingAddress const& binding) override;
    // } IEndpointMediator
    
    void Startup();
    void Shutdown();

    [[nodiscard]] SharedEndpoint GetEndpoint(Endpoint::Identifier identifier) const;
    [[nodiscard]] SharedEndpoint GetEndpoint(Protocol protocol, Operation operation) const;
    [[nodiscard]] ProtocolSet GetEndpointProtocols() const;
    [[nodiscard]] BindingAddress GetEndpointBinding(Endpoint::Identifier identifier) const;
    [[nodiscard]] std::size_t ActiveEndpointCount() const;
    [[nodiscard]] std::size_t ActiveProtocolCount() const;

    [[nodiscard]] bool ScheduleBind(BindingAddress const& binding);
    [[nodiscard]] bool ScheduleConnect(RemoteAddress const& address);
    [[nodiscard]] bool ScheduleConnect(RemoteAddress&& address);
    [[nodiscard]] bool ScheduleConnect(RemoteAddress&& address, Node::SharedIdentifier const& spIdentifier);

private:
    using EndpointMap = std::unordered_map<Endpoint::Identifier, SharedEndpoint>;
    using ShutdownCause = Event::Message<Event::Type::EndpointStopped>::Cause;
    using BindingCache = std::vector<std::pair<Endpoint::Identifier, BindingAddress>>;

    void Initialize(
        Configuration::Options::Endpoints const& endpoints,
        Event::SharedPublisher const& spEventPublisher,
        IPeerMediator* const pPeerMediator,
        IBootstrapCache const* const pBootstrapCache);
        
    void InitializeTCPEndpoints(
        Configuration::Options::Endpoint const& options,
        Event::SharedPublisher const& spEventPublisher,
        IPeerMediator* const pPeerMediator,
        IBootstrapCache const* const pBootstrapCache);

    void UpdateBindingCache(Endpoint::Identifier identifier, BindingAddress const& binding);

    void OnBindingFailed(RuntimeContext context);
    void OnEndpointShutdown(RuntimeContext context, ShutdownCause cause);
    void OnCriticalError();

    std::atomic_bool m_active;
    Event::SharedPublisher m_spEventPublisher;

    mutable std::shared_mutex m_endpointsMutex;
    EndpointMap m_endpoints;

    mutable std::shared_mutex m_cacheMutex;
    ProtocolSet m_protocols;
    BindingCache m_bindings;
};

//----------------------------------------------------------------------------------------------------------------------
