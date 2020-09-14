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
    enum class Phase { Information, Host, Connect, Close };

    explicit CTransformHandler(CBryptNode& instance);

    // IHandler{
    bool HandleMessage(AssociatedMessage const& associatedMessage) override;
    // }IHandler

    bool InfoHandler();
    bool HostHandler();
    bool ConnectHandler();
    bool CloseHandler();

};

//------------------------------------------------------------------------------------------------
