//----------------------------------------------------------------------------------------------------------------------
// File: Events.hpp
// Description: 
//----------------------------------------------------------------------------------------------------------------------
#pragma once
//----------------------------------------------------------------------------------------------------------------------
#include "BryptIdentifier/IdentifierTypes.hpp"
#include "Components/Network/EndpointTypes.hpp"
#include "Components/Network/Address.hpp"
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
    explicit ConnectEvent(
        RemoteAddress&& address,
        Node::SharedIdentifier const& session = nullptr);
    Node::SharedIdentifier const& GetNodeIdentifier() const;
    RemoteAddress const& GetRemoteAddress() const;

private:
    Node::SharedIdentifier m_spIdentifier;
    RemoteAddress m_address;
};

//----------------------------------------------------------------------------------------------------------------------

class Network::TCP::DispatchEvent : public Network::TCP::Event
{
public:
    DispatchEvent(std::shared_ptr<Session> const& session, std::string_view message);
    std::shared_ptr<Session> const& GetSession() const;
    std::string const& GetMessage() const;

private:
    std::shared_ptr<Session> m_spSession;
    std::string m_message;
};

//----------------------------------------------------------------------------------------------------------------------
