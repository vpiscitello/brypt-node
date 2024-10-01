
//----------------------------------------------------------------------------------------------------------------------
#include "Events.hpp"
//----------------------------------------------------------------------------------------------------------------------

Network::TCP::Event::Event(Instruction instruction)
    : m_instruction(instruction)
{
}

//----------------------------------------------------------------------------------------------------------------------

Network::Instruction const& Network::TCP::Event::GetInstruction() const
{
    return m_instruction;
}

//----------------------------------------------------------------------------------------------------------------------

Network::TCP::BindEvent::BindEvent(BindingAddress const& binding)
    : Event(Instruction::Bind)
    , m_binding(binding)
{
}

//----------------------------------------------------------------------------------------------------------------------

Network::BindingAddress const& Network::TCP::BindEvent::GetBinding() const
{
    return m_binding;
}

//----------------------------------------------------------------------------------------------------------------------

Network::TCP::ConnectEvent::ConnectEvent(RemoteAddress&& address, Node::SharedIdentifier const& spIdentifier)
    : Event(Instruction::Connect)
    , m_spIdentifier(spIdentifier)
    , m_address(std::move(address))
{
}

//----------------------------------------------------------------------------------------------------------------------

Node::SharedIdentifier const& Network::TCP::ConnectEvent::GetNodeIdentifier() const
{
    return m_spIdentifier;
}

//----------------------------------------------------------------------------------------------------------------------

Network::RemoteAddress&& Network::TCP::ConnectEvent::ReleaseAddress()
{
    return std::move(m_address);
}

//----------------------------------------------------------------------------------------------------------------------

Network::TCP::DisconnectEvent::DisconnectEvent(RemoteAddress&& address)
    : Event(Instruction::Connect)
    , m_address(std::move(address))
{
}

//----------------------------------------------------------------------------------------------------------------------

Network::RemoteAddress const& Network::TCP::DisconnectEvent::GetAddress() const
{
    return m_address;
}

//----------------------------------------------------------------------------------------------------------------------

Network::TCP::DispatchEvent::DispatchEvent(std::shared_ptr<Session> const& spSession, MessageVariant&& message)
    : Event(Instruction::Dispatch)
    , m_spSession(spSession)
    , m_message(std::move(message))
{
}

//----------------------------------------------------------------------------------------------------------------------

std::shared_ptr<Network::TCP::Session> const& Network::TCP::DispatchEvent::GetSession() const
{
    return m_spSession;
}

//----------------------------------------------------------------------------------------------------------------------

Network::MessageVariant&& Network::TCP::DispatchEvent::ReleaseMessage()
{
    return std::move(m_message);
}

//----------------------------------------------------------------------------------------------------------------------

bool Network::TCP::DispatchEvent::IsValid() const
{
    std::reference_wrapper<std::string const> message = std::holds_alternative<std::string>(m_message) ? 
            std::get<std::string>(m_message) :
            *std::get<Message::ShareablePack>(m_message);
    return (m_spSession && !message.get().empty());
}

//----------------------------------------------------------------------------------------------------------------------
