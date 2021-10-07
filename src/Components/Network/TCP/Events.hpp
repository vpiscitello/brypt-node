//----------------------------------------------------------------------------------------------------------------------
// File: Events.hpp
// Description: 
//----------------------------------------------------------------------------------------------------------------------
#pragma once
//----------------------------------------------------------------------------------------------------------------------
#include "BryptIdentifier/IdentifierTypes.hpp"
#include "Components/Network/Actions.hpp"
#include "Components/Network/Address.hpp"
#include "Components/Network/EndpointTypes.hpp"
//----------------------------------------------------------------------------------------------------------------------
#include <memory>
#include <string>
//----------------------------------------------------------------------------------------------------------------------

//----------------------------------------------------------------------------------------------------------------------
namespace Network::TCP {
//----------------------------------------------------------------------------------------------------------------------

class Session;

class Event;

class BindEvent;
class ConnectEvent;
class DisconnectEvent;
class DispatchEvent;

//----------------------------------------------------------------------------------------------------------------------
} // Network::TCP namespace
//----------------------------------------------------------------------------------------------------------------------

class Network::TCP::Event
{
public:
    explicit Event(Instruction instruction);
    Instruction const& GetInstruction() const;

private:
    Instruction const m_instruction;
};

//----------------------------------------------------------------------------------------------------------------------

class Network::TCP::BindEvent : public Network::TCP::Event
{
public:
    explicit BindEvent(BindingAddress const& binding);
    BindingAddress const& GetBinding() const;

private:
    BindingAddress m_binding;
};

//----------------------------------------------------------------------------------------------------------------------

class Network::TCP::ConnectEvent : public Network::TCP::Event
{
public:
    explicit ConnectEvent(RemoteAddress&& address, Node::SharedIdentifier const& session = nullptr);
    Node::SharedIdentifier const& GetNodeIdentifier() const;
    RemoteAddress&& ReleaseAddress();

private:
    Node::SharedIdentifier m_spIdentifier;
    RemoteAddress m_address;
};

//----------------------------------------------------------------------------------------------------------------------

class Network::TCP::DisconnectEvent : public Network::TCP::Event
{
public:
    explicit DisconnectEvent(RemoteAddress&& address);
    RemoteAddress const& GetAddress() const;

private:
    RemoteAddress m_address;
};

//----------------------------------------------------------------------------------------------------------------------

class Network::TCP::DispatchEvent : public Network::TCP::Event
{
public:
    DispatchEvent(std::shared_ptr<Session> const& session, MessageVariant&& message);
    std::shared_ptr<Session> const& GetSession() const;
    MessageVariant&& ReleaseMessage();
    bool IsValid() const;

private:
    std::shared_ptr<Session> m_spSession;
    MessageVariant m_message;
};

//----------------------------------------------------------------------------------------------------------------------
