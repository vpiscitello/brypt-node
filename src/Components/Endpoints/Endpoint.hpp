//------------------------------------------------------------------------------------------------
// File: Endpoint.hpp
// Description: Defines a set of communication methods for use on varying types of communication
// technologies. Currently supports ZMQ D, ZMQ StreamBridge, and TCP sockets.
//------------------------------------------------------------------------------------------------
#pragma once
//------------------------------------------------------------------------------------------------
#include "EndpointTypes.hpp"
#include "EndpointIdentifier.hpp"
#include "TechnologyType.hpp"
#include "../../Configuration/Configuration.hpp"
#include "../../Interfaces/EndpointMediator.hpp"
#include "../../Interfaces/MessageSink.hpp"
#include "../../Interfaces/PeerMediator.hpp"
#include "../../Utilities/NetworkUtils.hpp"
#include "../../Utilities/NodeUtils.hpp"
#include "zmq.hpp"
//------------------------------------------------------------------------------------------------
#include <any>
#include <atomic>
#include <condition_variable>
#include <cstdio>
#include <cstdlib>
#include <deque>
#include <memory>
#include <mutex>
#include <thread>
#include <string>
#include <string_view>
//------------------------------------------------------------------------------------------------

class CMessage;

//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
namespace Endpoints {
//------------------------------------------------------------------------------------------------

class CDirectEndpoint;
class CLoRaEndpoint;
class CStreamBridgeEndpoint;
class CTcpEndpoint;

std::unique_ptr<CEndpoint> Factory(
    TechnologyType technology,
    NodeUtils::NodeIdType id,
    std::string_view interface,
    Endpoints::OperationType operation,
    IEndpointMediator const* const pEndpointMediator,
    IPeerMediator* const pPeerMediator,
    IMessageSink* const pMessageSink);

//------------------------------------------------------------------------------------------------
} // Endpoint namespace
//------------------------------------------------------------------------------------------------

class CEndpoint {
public:
    enum class NetworkInstruction : std::uint8_t { Bind, Connect };

    CEndpoint(
        NodeUtils::NodeIdType id,
        std::string_view interface,
        Endpoints::OperationType operation,
        IEndpointMediator const* const pEndpointMediator,
        IPeerMediator* const pPeerMediator,
        IMessageSink* const pMessageSink,
        Endpoints::TechnologyType technology = Endpoints::TechnologyType::Invalid)
        : m_mutex()
        , m_identifier(CEndpointIdentifierGenerator::Instance().GetIdentifier())
        , m_technology(technology)
        , m_nodeIdentifier(id) 
        , m_interface(interface)
        , m_operation(operation)
        , m_pEndpointMediator(pEndpointMediator)
        , m_pPeerMediator(pPeerMediator)
        , m_pMessageSink(pMessageSink)
        , m_active(false)
        , m_terminate(false)
        , m_cv()
        , m_worker()
    {
        if (m_operation == Endpoints::OperationType::Invalid) {
            throw std::runtime_error("Endpoint must be provided and endpoint operation type!");
        }
    };

    virtual ~CEndpoint() = default; 

    virtual Endpoints::TechnologyType GetInternalType() const = 0;
    virtual std::string GetProtocolType() const = 0;
    virtual std::string GetEntry() const = 0;
    virtual std::string GetURI() const = 0;

    virtual void ScheduleBind(std::string_view binding) = 0;
    virtual void ScheduleConnect(std::string_view entry) = 0;
    virtual void Startup() = 0;

	virtual bool ScheduleSend(CMessage const& message) = 0;
	virtual bool ScheduleSend(NodeUtils::NodeIdType id, std::string_view message) = 0;
    
	virtual bool Shutdown() = 0;

    bool IsActive() const;
    Endpoints::EndpointIdType GetIdentifier() const;
    Endpoints::OperationType GetOperation() const;

protected:
    void PublishPeerConnection(CPeer const& peer);
    void UnpublishPeerConnection(CPeer const& peer);

    using EventDeque = std::deque<std::any>;

    mutable std::mutex m_mutex;

    Endpoints::EndpointIdType const m_identifier;
    Endpoints::TechnologyType const m_technology;

	NodeUtils::NodeIdType const m_nodeIdentifier;
    std::string m_interface;
	Endpoints::OperationType const m_operation;

    IEndpointMediator const* const m_pEndpointMediator;
    IPeerMediator* const m_pPeerMediator;
    IMessageSink* const m_pMessageSink;

	std::atomic_bool m_active;
    std::atomic_bool m_terminate;
    mutable std::condition_variable m_cv;
    std::thread m_worker;
};

//------------------------------------------------------------------------------------------------
