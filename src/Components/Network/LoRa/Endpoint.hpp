//----------------------------------------------------------------------------------------------------------------------
// File: Endpoint.hpp
// Description:
//----------------------------------------------------------------------------------------------------------------------
#pragma once
//----------------------------------------------------------------------------------------------------------------------
#include "Components/Event/SharedPublisher.hpp"
#include "Components/Network/Endpoint.hpp"
#include "Components/Network/MessageScheduler.hpp"
#include "Components/Network/Protocol.hpp"
//----------------------------------------------------------------------------------------------------------------------

namespace Network { class BindingAddress; class RemoteAddress; }

//----------------------------------------------------------------------------------------------------------------------
namespace Network::LoRa {
//----------------------------------------------------------------------------------------------------------------------

class Endpoint;

//----------------------------------------------------------------------------------------------------------------------
} // LoRa namespace
//----------------------------------------------------------------------------------------------------------------------

class Network::LoRa::Endpoint : public Network::IEndpoint
{
public:
    Endpoint(Operation operation, Event::SharedPublisher const& spEventPublisher);
    ~Endpoint() override;

    // IEndpoint{
    virtual Protocol GetProtocol() const override;
    virtual std::string GetScheme() const override;
    virtual BindingAddress GetBinding() const override;
    virtual bool IsActive() const override;

    virtual void Startup() override;
    virtual bool Shutdown() override;
    
    [[nodiscard]] virtual bool ScheduleBind(BindingAddress const& binding) override;
    [[nodiscard]] virtual bool ScheduleConnect(RemoteAddress const& address) override;
    [[nodiscard]] virtual bool ScheduleConnect(RemoteAddress&& address) override;
    [[nodiscard]] virtual bool ScheduleConnect(
        RemoteAddress&& address, Node::SharedIdentifier const& spIdentifier) override;
    [[nodiscard]] virtual bool ScheduleSend(Node::Identifier const& identifier, std::string&& message) override;
    [[nodiscard]] virtual bool ScheduleSend(
        Node::Identifier const& identifier, Message::ShareablePack const& spSharedPack) override;
    [[nodiscard]] virtual bool ScheduleSend(Node::Identifier const& identifier, MessageVariant&& message) override;
    // } IEndpoint
};

//----------------------------------------------------------------------------------------------------------------------
