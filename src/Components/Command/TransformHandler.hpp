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
class Command::CTransform : public Command::CHandler {
public:
    CTransform(CNode& instance, std::weak_ptr<CState> const& state);
    ~CTransform() override {};

    // CHandler{
    bool HandleMessage(CMessage const& message) override;
    // }CHandler

    bool InfoHandler();
    bool HostHandler();
    bool ConnectHandler();
    bool CloseHandler();

private:
    enum class Phase { INFO, HOST, CONNECT, CLOSE };
};

//------------------------------------------------------------------------------------------------
