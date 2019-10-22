//------------------------------------------------------------------------------------------------
// File: MessageQueue.hpp
// Description:
//------------------------------------------------------------------------------------------------
#include "MessageQueue.hpp"
//------------------------------------------------------------------------------------------------
#include <algorithm>
//------------------------------------------------------------------------------------------------

CMessageQueue::CMessageQueue()
	: m_incoming()
	, m_outgoing()
	, m_pipes()
{
}

//------------------------------------------------------------------------------------------------

CMessageQueue::~CMessageQueue()
{
}

//------------------------------------------------------------------------------------------------

bool CMessageQueue::AddManagedPipe(NodeUtils::NodeIdType const& id)
{
	if (auto const itr = m_pipes.find(id); itr != m_pipes.end()) {
		NodeUtils::printo("Pipe already being watched", NodeUtils::PrintType::MQUEUE);
		return false;
	}

	std::string const filename = "./tmp/" + id + ".pipe";
	std::ifstream file(filename);

	NodeUtils::printo("Pushing " + filename, NodeUtils::PrintType::MQUEUE);

	if (file.fail()){
		NodeUtils::printo("Failed to open pipe file!", NodeUtils::PrintType::MQUEUE);
		return false;
	}

	file.close();

	m_pipes.emplace(id, filename);

	NodeUtils::printo("Pipes being watched: " + std::to_string(m_pipes.size()), NodeUtils::PrintType::MQUEUE);

	return true;
}

//------------------------------------------------------------------------------------------------

bool CMessageQueue::RemoveManagedPipe(NodeUtils::NodeIdType const& id)
{
	if (auto const itr = m_pipes.find(id); itr != m_pipes.end()) {
		m_pipes.erase(itr);
		return true;
	}
	return false;
}

//------------------------------------------------------------------------------------------------

bool CMessageQueue::PushOutgoingMessage(NodeUtils::NodeIdType const& id, CMessage const& message)
{
	std::string filename = "./tmp/" + id + ".pipe";

	NodeUtils::printo("MessageQueued for " + filename, NodeUtils::PrintType::MQUEUE);

	if (auto const itr = m_pipes.find(id); itr == m_pipes.end()) {
		return false;
	}

	m_outgoing.push(message);
	return true;
}

//------------------------------------------------------------------------------------------------

bool CMessageQueue::PushOutgoingMessages()
{
	CheckPipes();

	std::string packet;
	while (m_outgoing.size() > 0) {
		CMessage message = m_outgoing.front();
		m_outgoing.pop();

		std::string const filename = "./tmp/" + message.GetDestinationId() + ".pipe";
		packet = message.GetPack();
		NodeUtils::printo("Pushing message for " + filename, NodeUtils::PrintType::MQUEUE);

		std::ofstream file(filename, std::ios::out | std::ios::binary);
		if (file.good()) {
			file.write(packet.c_str(), packet.size());
		}

		file.close();
	}

	return 0;
}

//------------------------------------------------------------------------------------------------

bool CMessageQueue::CheckPipes()
{
	for (auto itr = m_pipes.begin(); itr != m_pipes.end(); ++itr) {

		std::string const& filename = itr->second;
		NodeUtils::printo("Checking " + filename, NodeUtils::PrintType::MQUEUE);

		std::ifstream file(filename);
		if (file.fail()) {
			continue;
		}

		char c;
		std::string line = std::string();
		while(file.get(c)){
			line.append(sizeof(c), c);
		}
		file.close();

		if (line.empty()) {
			NodeUtils::printo("No message in checked pipe", NodeUtils::PrintType::MQUEUE);
			continue;
		}

		try {
			CMessage message(line); // TODO: Make a safe unpacking method that doesn't throw exceptions
			m_incoming.push(message);
		} catch(...) {
			NodeUtils::printo("Message in queue not formatted properly", NodeUtils::PrintType::MQUEUE);
		}

		std::ofstream clear(filename, std::ios::out | std::ios::trunc);
		clear.close();
	}

	return true;
}

//------------------------------------------------------------------------------------------------

std::optional<CMessage> CMessageQueue::PopIncomingMessage()
{
	if (!m_incoming.empty()) {
		CMessage message = m_incoming.front();
		m_incoming.pop();

		NodeUtils::printo(std::to_string(m_incoming.size()) + " left in incoming queue", NodeUtils::PrintType::MQUEUE);

		return std::move(message);
	}

	return {};
}

//------------------------------------------------------------------------------------------------