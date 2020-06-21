//------------------------------------------------------------------------------------------------
// File: LoRaEndpoint.hpp
// Description:
//------------------------------------------------------------------------------------------------
#pragma once
//------------------------------------------------------------------------------------------------
#include "Endpoint.hpp"
#include "TechnologyType.hpp"
//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
namespace LoRa {
//------------------------------------------------------------------------------------------------
//------------------------------------------------------------------------------------------------
} // CLoRa namespace
//------------------------------------------------------------------------------------------------

class Endpoints::CLoRaEndpoint : public CEndpoint {
public:
    constexpr static std::string_view ProtocolType = "LoRa";
    constexpr static TechnologyType InternalType = TechnologyType::LoRa;

    CLoRaEndpoint(
        NodeUtils::NodeIdType id,
        std::string_view interface,
        OperationType operation,
        IMessageSink* const messageSink);
    ~CLoRaEndpoint() override;

    // CEndpoint{
    TechnologyType GetInternalType() const override;
    std::string GetProtocolType() const override;
    std::string GetEntry() const override;

    void ScheduleBind(std::string_view binding) override;
    void ScheduleConnect(std::string_view entry) override;
    void Startup() override;

    bool ScheduleSend(CMessage const& message) override;
    bool ScheduleSend(NodeUtils::NodeIdType id, std::string_view message) override;

    bool Shutdown() override;
    // }CEndpoint

private:
    void Spawn();

};

//------------------------------------------------------------------------------------------------
