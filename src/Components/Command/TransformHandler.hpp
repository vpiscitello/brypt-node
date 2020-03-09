//------------------------------------------------------------------------------------------------
// File: TransformHandler.hpp
// Description:
//------------------------------------------------------------------------------------------------
#pragma once
//------------------------------------------------------------------------------------------------
#include "Handler.hpp"
//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
// Description: Handle Requests regarding Transforming Node type
//------------------------------------------------------------------------------------------------
class Command::CTransformHandler : public Command::IHandler {
public:
    explicit CTransformHandler(CBryptNode& instance);

    // IHandler{
    bool HandleMessage(CMessage const& message) override;
    // }IHandler

    bool InfoHandler();
    bool HostHandler();
    bool ConnectHandler();
    bool CloseHandler();

private:
    enum class Phase { Information, Host, Connect, Close };
};

//------------------------------------------------------------------------------------------------
