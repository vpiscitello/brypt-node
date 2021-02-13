
//------------------------------------------------------------------------------------------------
#include "Events.hpp"
//------------------------------------------------------------------------------------------------

Network::TCP::Event::Event(Instruction instruction)
    : m_instruction(instruction)
{
}

//------------------------------------------------------------------------------------------------

Network::Instruction const& Network::TCP::Event::GetInstruction() const
{
    return m_instruction;
}

//------------------------------------------------------------------------------------------------

Network::TCP::BindEvent::BindEvent(BindingAddress const& binding)
    : Event(Instruction::Bind)
    , m_binding(binding)
{
}

//------------------------------------------------------------------------------------------------

Network::BindingAddress const& Network::TCP::BindEvent::GetBinding() const
{
    return m_binding;
}

//------------------------------------------------------------------------------------------------

Network::TCP::ConnectEvent::ConnectEvent(
    RemoteAddress&& address,
    BryptIdentifier::SharedContainer const& spIdentifier)
    : Event(Instruction::Connect)
    , m_spIdentifier(spIdentifier)
    , m_address(std::move(address))
{
}

//------------------------------------------------------------------------------------------------

BryptIdentifier::SharedContainer const& Network::TCP::ConnectEvent::GetBryptIdentifier() const
{
    return m_spIdentifier;
}

//------------------------------------------------------------------------------------------------

Network::RemoteAddress const& Network::TCP::ConnectEvent::GetRemoteAddress() const
{
    return m_address;
}

//------------------------------------------------------------------------------------------------

Network::TCP::DispatchEvent::DispatchEvent(
    std::shared_ptr<Session> const& spSession, std::string_view message)
    : Event(Instruction::Dispatch)
    , m_spSession(spSession)
    , m_message(message)
{
}

//------------------------------------------------------------------------------------------------

std::shared_ptr<Network::TCP::Session> const& Network::TCP::DispatchEvent::GetSession() const
{
    return m_spSession;
}

//------------------------------------------------------------------------------------------------

std::string const& Network::TCP::DispatchEvent::GetMessage() const
{
    return m_message;
}

//------------------------------------------------------------------------------------------------
