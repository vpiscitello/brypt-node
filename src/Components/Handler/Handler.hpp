//----------------------------------------------------------------------------------------------------------------------
// File: Handler.hpp
// Description: Defines a set of handler types for messages and the appropriate responses based
// on the phase that the communication is currently in.
//----------------------------------------------------------------------------------------------------------------------
#pragma once
//----------------------------------------------------------------------------------------------------------------------
#include "HandlerDefinitions.hpp"
#include "BryptIdentifier/BryptIdentifier.hpp"
#include "BryptMessage/MessageDefinitions.hpp"
#include "Components/MessageControl/AssociatedMessage.hpp"
//----------------------------------------------------------------------------------------------------------------------
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
//----------------------------------------------------------------------------------------------------------------------

namespace spdlog { class logger; }

namespace Node { class Core; }
namespace Peer { class Proxy; }

class ApplicationMessage;

//----------------------------------------------------------------------------------------------------------------------
namespace Handler {
//----------------------------------------------------------------------------------------------------------------------

class IHandler;

class Connect;
class Election;
class Information;
class Query;

std::unique_ptr<IHandler> Factory(Handler::Type handlerType, Node::Core& instance);

using Map = std::unordered_map<Handler::Type, std::unique_ptr<IHandler>>;

//----------------------------------------------------------------------------------------------------------------------
} // Handler namespace
//----------------------------------------------------------------------------------------------------------------------

//----------------------------------------------------------------------------------------------------------------------
// Description:
//----------------------------------------------------------------------------------------------------------------------
class Handler::IHandler
{
public:
    IHandler(Handler::Type type, Node::Core& instance);
    virtual ~IHandler() = default;
    
    virtual Handler::Type GetType() const final;

    virtual bool HandleMessage(AssociatedMessage const& message) = 0;

protected:
    virtual void SendClusterNotice(
        std::weak_ptr<Peer::Proxy> const& wpPeerProxy,
        ApplicationMessage const& request,
        std::string_view noticeData,
        std::uint8_t noticePhase,
        std::uint8_t responsePhase,
        std::optional<std::string> optResponseData) final;

    virtual void SendNetworkNotice(
        std::weak_ptr<Peer::Proxy> const& wpPeerProxy,
        ApplicationMessage const& request,
        std::string_view noticeData,
        std::uint8_t noticePhase,
        std::uint8_t responsePhase,
        std::optional<std::string> optResponseData) final;

    virtual void SendResponse(
        std::weak_ptr<Peer::Proxy> const& wpPeerProxy,
        ApplicationMessage const& request,
        std::string_view responseData,
        std::uint8_t responsePhase) final;
        
    Handler::Type m_type;
    Node::Core& m_instance;
    std::shared_ptr<spdlog::logger> m_logger;

private: 
    virtual void SendNotice(
        std::weak_ptr<Peer::Proxy> const& wpPeerProxy,
        ApplicationMessage const& request,
        Message::Destination destination,
        std::string_view noticeData,
        std::uint8_t noticePhase,
        std::uint8_t responsePhase,
        std::optional<std::string> optResponseData) final;
};

//----------------------------------------------------------------------------------------------------------------------
