//------------------------------------------------------------------------------------------------
// File: MessageBuilder.cpp
// Description:
//------------------------------------------------------------------------------------------------
#include "MessageBuilder.hpp"
#include "MessageSecurity.hpp"
#include "PackUtils.hpp"
//------------------------------------------------------------------------------------------------

CMessageBuilder::CMessageBuilder()
    : m_message()
{
}

//------------------------------------------------------------------------------------------------

CMessageBuilder& CMessageBuilder::SetMessageContext(CMessageContext const& context)
{
	m_message.m_context = context;
	return *this;
}

//------------------------------------------------------------------------------------------------

CMessageBuilder& CMessageBuilder::SetSource(NodeUtils::NodeIdType id)
{
	m_message.m_source = id;
	return *this;
}

//------------------------------------------------------------------------------------------------

CMessageBuilder& CMessageBuilder::SetDestination(NodeUtils::NodeIdType id)
{
	m_message.m_destination = id;
	return *this;
}

//------------------------------------------------------------------------------------------------

CMessageBuilder& CMessageBuilder::SetDestination(ReservedIdentifiers id)
{
	m_message.m_destination = static_cast<NodeUtils::NodeIdType>(id);
	return *this;
}

//------------------------------------------------------------------------------------------------

CMessageBuilder& CMessageBuilder::BindAwaitingKey(
    Message::AwaitBinding binding,
    NodeUtils::ObjectIdType key)
{
	m_message.m_optBoundAwaitingKey = Message::BoundAwaitingKey(binding, key);
	return *this;
}

//------------------------------------------------------------------------------------------------

CMessageBuilder& CMessageBuilder::SetCommand(Command::Type type, std::uint8_t phase)
{
	m_message.m_command = type;
	m_message.m_phase = phase;
	return *this;
}

//------------------------------------------------------------------------------------------------

CMessageBuilder& CMessageBuilder::SetData(std::string_view data, NodeUtils::NetworkNonce nonce)
{
	Message::Buffer buffer(data.begin(), data.end());
    return SetData(buffer, nonce);
}

//------------------------------------------------------------------------------------------------

CMessageBuilder& CMessageBuilder::SetData(Message::Buffer const& buffer, NodeUtils::NetworkNonce nonce)
{
	auto const optData = MessageSecurity::Encrypt(buffer, buffer.size(), nonce);
	if(optData) {
		m_message.m_data = *optData;
        m_message.m_nonce = nonce;
	}
    return *this;
}

//------------------------------------------------------------------------------------------------

CMessageBuilder& CMessageBuilder::FromPack(Message::Buffer const& buffer)
{
    return FromPack(reinterpret_cast<char const*>(buffer.data()));
}

//------------------------------------------------------------------------------------------------

CMessageBuilder& CMessageBuilder::FromPack(std::string_view pack)
{
    if (pack.empty()) {
        return *this;
    }
    
    auto const status = MessageSecurity::Verify(pack);
    if (status != MessageSecurity::VerificationStatus::Success) {
        return *this;
    }

    Unpack(PackUtils::Z85Decode(pack));
    return *this;
}

//------------------------------------------------------------------------------------------------

CMessage&& CMessageBuilder::Build()
{
    return std::move(m_message);
}

//------------------------------------------------------------------------------------------------

OptionalMessage CMessageBuilder::ValidatedBuild()
{
    if (m_message.Validate() != CMessage::ValidationStatus::Success) {
        return {};
    }
    return std::move(m_message);
}

//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
// Description: Unpack the raw message string into the Message class variables.
//------------------------------------------------------------------------------------------------
void CMessageBuilder::Unpack(Message::Buffer const& buffer)
{
	std::uint32_t position = 0;

    try {
        PackUtils::UnpackChunk(buffer, position, m_message.m_source);
        PackUtils::UnpackChunk(buffer, position, m_message.m_destination);

        Message::AwaitBinding binding = Message::AwaitBinding::None;
        PackUtils::UnpackChunk(buffer, position, binding);
        if (binding != Message::AwaitBinding::None) {
            NodeUtils::ObjectIdType key = 0;
            PackUtils::UnpackChunk(buffer, position, key);
            m_message.m_optBoundAwaitingKey = Message::BoundAwaitingKey(binding, key);
        }
        
        PackUtils::UnpackChunk(buffer, position, m_message.m_command);
        PackUtils::UnpackChunk(buffer, position, m_message.m_phase);
        PackUtils::UnpackChunk(buffer, position, m_message.m_nonce);

        std::uint16_t dataLength = 0;
        PackUtils::UnpackChunk(buffer, position, dataLength);
        PackUtils::UnpackChunk(buffer, position, m_message.m_data, dataLength);

        std::uint64_t timestamp;
        PackUtils::UnpackChunk(buffer, position, timestamp);
        m_message.m_timepoint = TimeUtils::Timepoint(TimeUtils::TimePeriod(timestamp));
    } catch (...) {
        NodeUtils::printo("[Node] Message failed to unpack.", NodeUtils::PrintType::Node);
    }
}

//------------------------------------------------------------------------------------------------
