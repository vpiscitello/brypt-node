//----------------------------------------------------------------------------------------------------------------------
// File: LoRaEndpoint.hpp
// Description:
//----------------------------------------------------------------------------------------------------------------------
#pragma once
//----------------------------------------------------------------------------------------------------------------------
#include "Components/Network/Endpoint.hpp"
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
    explicit Endpoint(Operation operation);
    ~Endpoint() override;

    // IEndpoint{
    virtual Protocol GetProtocol() const override;
    virtual std::string GetScheme() const override;
    virtual BindingAddress GetBinding() const override;
    virtual bool IsActive() const override;

    virtual void Startup() override;
    virtual bool Shutdown() override;
    
    virtual void ScheduleBind(BindingAddress const& binding) override;
    virtual void ScheduleConnect(RemoteAddress const& address) override;
    virtual void ScheduleConnect(RemoteAddress&& address) override;
    virtual void ScheduleConnect(
        RemoteAddress&& address,
        Node::SharedIdentifier const& spIdentifier) override;
    virtual bool ScheduleSend(
        Node::Identifier const& identifier,
        std::string_view message) override;
    // } IEndpoint
};

//----------------------------------------------------------------------------------------------------------------------
