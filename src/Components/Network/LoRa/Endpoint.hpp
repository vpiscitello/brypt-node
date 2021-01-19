//------------------------------------------------------------------------------------------------
// File: LoRaEndpoint.hpp
// Description:
//------------------------------------------------------------------------------------------------
#pragma once
//------------------------------------------------------------------------------------------------
#include "Components/Network/Endpoint.hpp"
#include "Components/Network/Protocol.hpp"
//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
namespace Network::LoRa {
//------------------------------------------------------------------------------------------------

class Endpoint;

//------------------------------------------------------------------------------------------------
} // LoRa namespace
//------------------------------------------------------------------------------------------------

class Network::LoRa::Endpoint : public Network::IEndpoint
{
public:
    constexpr static std::string_view Scheme = "lora://";
    constexpr static Protocol ProtocolType = Protocol::LoRa;
    constexpr static std::string_view ProtocolString = "LoRa";

    Endpoint(std::string_view interface, Operation operation);
    ~Endpoint() override;

    // IEndpoint{
    virtual Protocol GetProtocol() const override;
    virtual std::string GetProtocolString() const override;
    virtual std::string GetEntry() const override;
    virtual std::string GetURI() const override;
    virtual bool IsActive() const override;

    virtual void Startup() override;
    virtual bool Shutdown() override;
    
    virtual void ScheduleBind(std::string_view binding) override;
    virtual void ScheduleConnect(std::string_view entry) override;
    virtual void ScheduleConnect(
        BryptIdentifier::SharedContainer const& spIdentifier, std::string_view entry) override;
    virtual bool ScheduleSend(
        BryptIdentifier::Container const& identifier,
        std::string_view message) override;
    // } IEndpoint
};

//------------------------------------------------------------------------------------------------
