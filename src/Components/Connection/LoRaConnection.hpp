//------------------------------------------------------------------------------------------------
// File: LoRaConnection.hpp
// Description:
//------------------------------------------------------------------------------------------------
#include "Connection.hpp"
//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
namespace Connection {
//------------------------------------------------------------------------------------------------
//------------------------------------------------------------------------------------------------
namespace LoRa {
//------------------------------------------------------------------------------------------------
//------------------------------------------------------------------------------------------------
} // CLoRa namespace
//------------------------------------------------------------------------------------------------
} // Connection namespace
//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------

class Connection::CLoRa : public CConnection {
public:
    CLoRa(
        IMessageSink* const messageSink,
        Configuration::TConnectionOptions const& options);
    ~CLoRa() override;

    void whatami() override;
    std::string const& GetProtocolType() const override;
    NodeUtils::TechnologyType GetInternalType() const override;

    void Spawn() override;
    void Worker() override;

    void HandleProcessedMessage(CMessage const& message) override;
    void Send(CMessage const& message) override;
    void Send(std::string_view message) override;
    std::optional<std::string> Receive(std::int32_t flag = 0) override;

    void PrepareForNext() override;
    bool Shutdown() override;

private:

};

//------------------------------------------------------------------------------------------------
