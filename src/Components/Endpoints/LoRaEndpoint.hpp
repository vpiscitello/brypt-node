//------------------------------------------------------------------------------------------------
// File: LoRaEndpoint.hpp
// Description:
//------------------------------------------------------------------------------------------------
#pragma once
//------------------------------------------------------------------------------------------------
#include "Endpoint.hpp"
//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
namespace LoRa {
//------------------------------------------------------------------------------------------------
//------------------------------------------------------------------------------------------------
} // CLoRa namespace
//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------

class Endpoints::CLoRaEndpoint : public CEndpoint {
public:
    constexpr static std::string_view ProtocolType = "LoRa";
    constexpr static NodeUtils::TechnologyType InternalType = NodeUtils::TechnologyType::LoRa;

    CLoRaEndpoint(
        IMessageSink* const messageSink,
        Configuration::TEndpointOptions const& options);
    ~CLoRaEndpoint() override;

    // CEndpoint{
    NodeUtils::TechnologyType GetInternalType() const override;
    std::string GetProtocolType() const override;
    std::string GetEntry() const override;

    void ScheduleBind(std::string_view binding) override;
    void ScheduleConnect(std::string_view entry) override;
    void Startup() override;

    void HandleProcessedMessage(NodeUtils::NodeIdType id, CMessage const& message) override;
    void ScheduleSend(NodeUtils::NodeIdType id, CMessage const& message) override;
    void ScheduleSend(NodeUtils::NodeIdType id, std::string_view message) override;

    bool Shutdown() override;
    // }CEndpoint

private:
    void Spawn();

};

//------------------------------------------------------------------------------------------------
