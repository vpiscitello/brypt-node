//------------------------------------------------------------------------------------------------
// File: Message.hpp
// Description:
//------------------------------------------------------------------------------------------------
#pragma once
//------------------------------------------------------------------------------------------------
#include "NodeUtils.hpp"
//------------------------------------------------------------------------------------------------
#include <chrono>
#include <functional>
#include <memory>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
//------------------------------------------------------------------------------------------------
#include <openssl/conf.h>
#include <openssl/err.h>
#include <openssl/sha.h>
#include <openssl/evp.h>
#include <openssl/hmac.h>
//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
namespace {
//------------------------------------------------------------------------------------------------
namespace local {
//------------------------------------------------------------------------------------------------
constexpr std::uint8_t BeginByte = 2;
constexpr std::uint8_t EndByte = 4;
constexpr std::uint8_t SeperatorByte = 29;

constexpr std::string_view HashMethod = "blake2s256";
//------------------------------------------------------------------------------------------------
} // local namespace
//------------------------------------------------------------------------------------------------
} // namespace
//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
namespace Base64 {
//------------------------------------------------------------------------------------------------
// Base64 Encode/Decode source: https://github.com/ReneNyffenegger/cpp-base64
constexpr std::string_view const Characters = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
static inline bool IsValid(unsigned char c) {
	return (isalnum(c) || (c == '+') || (c == '/'));
}
//------------------------------------------------------------------------------------------------
} // Base64 namespace
//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------

class CMessage {
public:
	CMessage()
		: m_raw(std::string())
		, m_sourceId(std::string())
		, m_destinationId(std::string())
		, m_awaitId({})
		, m_command(NodeUtils::CommandType::NONE)
		, m_phase(std::numeric_limits<std::int32_t>::min())
		, m_data(std::string())
		, m_length(0)
		, m_timepoint(NodeUtils::GetSystemTimePoint())
		, m_key(NodeUtils::NETWORK_KEY)
		, m_nonce(0)
		, m_response(nullptr)
		, m_token(std::string())
	{
	}

	//------------------------------------------------------------------------------------------------

	CMessage(
		NodeUtils::NodeIdType const& sourceId,
		NodeUtils::NodeIdType const& destinationId,
		NodeUtils::CommandType const& command,
		std::int32_t phase,
		std::string const& data,
		std::uint32_t nonce)
		: m_raw(std::string())
		, m_sourceId(sourceId)
		, m_destinationId(destinationId)
		, m_awaitId(std::string())
		, m_command(command)
		, m_phase(phase)
		, m_data(std::string())
		, m_length(0)
		, m_timepoint()
		, m_key(NodeUtils::NETWORK_KEY)
		, m_nonce(nonce)
		, m_response(nullptr)
		, m_token(std::string())
	{
		auto const optData = Encrypt(data, data.size());
		if(optData) {
			m_data = *optData;
		}
	}

	//------------------------------------------------------------------------------------------------

	explicit CMessage(std::string const& raw)
		: m_raw(std::string())
		, m_sourceId(std::string())
		, m_destinationId(std::string())
		, m_awaitId({})
		, m_command(NodeUtils::CommandType::NONE)
		, m_phase(std::numeric_limits<std::int32_t>::min())
		, m_data(std::string())
		, m_length(0)
		, m_timepoint(NodeUtils::GetSystemTimePoint())
		, m_key(NodeUtils::NETWORK_KEY)
		, m_nonce(0)
		, m_response(nullptr)
		, m_token(std::string())
	{
		Unpack(Base64Decode(raw, raw.size()));
	}

	//------------------------------------------------------------------------------------------------

	CMessage(CMessage const& other)
		: m_raw(other.m_raw)
		, m_sourceId(other.m_sourceId)
		, m_destinationId(other.m_destinationId)
		, m_awaitId(other.m_awaitId)
		, m_command(other.m_command)
		, m_phase(other.m_phase)
		, m_data(other.m_data)
		, m_length(other.m_length)
		, m_timepoint(other.m_timepoint)
		, m_key(other.m_key)
		, m_nonce(other.m_nonce)
		, m_response(nullptr)
		, m_token(other.m_token)
	{
		if (other.m_response) {
			m_response = std::make_unique<CMessage>(*other.m_response.get());
		}
	}

	//------------------------------------------------------------------------------------------------

	CMessage(CMessage&& other)
		: m_raw(other.m_raw)
		, m_sourceId(other.m_sourceId)
		, m_destinationId(other.m_destinationId)
		, m_awaitId(other.m_awaitId)
		, m_command(other.m_command)
		, m_phase(other.m_phase)
		, m_data(other.m_data)
		, m_length(other.m_length)
		, m_timepoint(other.m_timepoint)
		, m_key(other.m_key)
		, m_nonce(other.m_nonce)
		, m_response(nullptr)
		, m_token(other.m_token)
	{
		if (other.m_response) {
			m_response = std::move(other.m_response);
		}
	}

	//------------------------------------------------------------------------------------------------

	CMessage& operator=(const CMessage& other)
	{
		m_raw = other.m_raw;
		m_sourceId = other.m_sourceId;
		m_destinationId = other.m_destinationId;
		m_awaitId = other.m_awaitId;
		m_command = other.m_command;
		m_phase = other.m_phase;
		m_data = other.m_data;
		m_length = other.m_length;
		m_timepoint = other.m_timepoint;
		m_key = other.m_key;
		m_nonce = other.m_nonce;
		m_token = other.m_token;
		m_response = std::make_unique<CMessage>(*other.m_response.get());

		return *this;
	}

	//------------------------------------------------------------------------------------------------

	~CMessage()
	{
	}

	//------------------------------------------------------------------------------------------------

	NodeUtils::NodeIdType const& GetSourceId() const
	{
		return m_sourceId;
	}

	//------------------------------------------------------------------------------------------------

	NodeUtils::NodeIdType const& GetDestinationId() const
	{
		return m_destinationId;
	}

	//------------------------------------------------------------------------------------------------

	std::optional<NodeUtils::ObjectIdType> const& GetAwaitId() const
	{
		return m_awaitId;
	}

	//------------------------------------------------------------------------------------------------

	NodeUtils::CommandType const& GetCommand() const
	{
		return m_command;
	}

	//------------------------------------------------------------------------------------------------

	std::uint32_t GetPhase() const
	{
		return m_phase;
	}

	//------------------------------------------------------------------------------------------------

	std::string const& GetData() const
	{
		return m_data;
	}

	//------------------------------------------------------------------------------------------------

	NodeUtils::TimePoint const& GetSystemTimePoint() const
	{
		return m_timepoint;
	}

	//------------------------------------------------------------------------------------------------

	std::uint32_t GetNonce() const
	{
		return m_nonce;
	}

	//------------------------------------------------------------------------------------------------

	std::string GetPack() const
	{
		if (m_raw.empty()){
			Pack();
		}
		std::string const signedPack = m_raw + m_token;
		std::string const encodedPack = Base64Encode(signedPack, signedPack.size());
		return encodedPack;
	}

	//------------------------------------------------------------------------------------------------

	std::optional<std::string> GetResponse() const
	{
		if (m_response) {
			return m_response->GetPack();
		}
		return {};
	}

	//------------------------------------------------------------------------------------------------

	void SetRaw(std::string const& raw)
	{
		m_raw = raw;
	}

	//------------------------------------------------------------------------------------------------

	void SetSourceId(NodeUtils::NodeIdType const& sourceId)
	{
		m_sourceId = sourceId;
	}

	//------------------------------------------------------------------------------------------------

	void SetDestinationId(NodeUtils::NodeIdType const& destinationId)
	{
		m_destinationId = destinationId;
	}

	//------------------------------------------------------------------------------------------------

	void SetCommand(NodeUtils::CommandType const& command, std::int32_t phase)
	{
		m_command = command;
		m_phase = phase;
	}

	//------------------------------------------------------------------------------------------------

	void SetData(std::string const& data)
	{
		m_data = data;
	}

	//------------------------------------------------------------------------------------------------

	void SetNonce(std::uint32_t nonce)
	{
		m_nonce = nonce;
	}

	//------------------------------------------------------------------------------------------------

	void SetTimestamp()
	{
		m_timepoint = NodeUtils::GetSystemTimePoint();
	}

	//------------------------------------------------------------------------------------------------

	//------------------------------------------------------------------------------------------------
	// Description: Set the message Response provided the data content and sending Node ID.
	//------------------------------------------------------------------------------------------------
	void SetResponse(NodeUtils::NodeIdType const& sourceId, std::string const& data)
	{
		if (m_response == nullptr) {
			m_response = std::make_unique<CMessage>(sourceId, m_sourceId, m_command, m_phase + 1, data, m_nonce + 1);
		} else {
			m_response->SetSourceId(sourceId);
			m_response->SetDestinationId(m_sourceId);
			m_response->SetCommand(m_command, m_phase + 1);
			m_response->SetData(data);
			m_response->SetNonce(m_nonce + 1);
		}
	}

	//------------------------------------------------------------------------------------------------

	//------------------------------------------------------------------------------------------------
	// Description: Wrap a string message chunk with seperators.
	//------------------------------------------------------------------------------------------------
	std::string PackChunk(std::string const& chunk) const
	{
		std::string packed;
		// packed.append(1, 2);	// Start of telemetry chunk
		packed.append(chunk);
		// packed.append(1, 3);	// End of telemetry chunk
		packed.append(1, local::SeperatorByte);	// Telemetry seperator
		return packed;
	}

	//------------------------------------------------------------------------------------------------

	//------------------------------------------------------------------------------------------------
	// Description: Wrap an std::uint32_t message chunk with seperators.
	//------------------------------------------------------------------------------------------------
	std::string PackChunk(std::uint32_t chunk) const
	{
		std::string packed;
		std::stringstream stream;
		stream.clear();
		// packed.append(1, 2);	// Start of telemetry chunk
		stream << chunk;
		packed.append(stream.str());
		// packed.append(1, 3);	// End of telemetry chunk
		packed.append(1, local::SeperatorByte);	// Telemetry seperator
		return packed;
	}

	//------------------------------------------------------------------------------------------------

	//------------------------------------------------------------------------------------------------
	// Description: Pack the Message class values into a single raw string.
	//------------------------------------------------------------------------------------------------
	void Pack() const
	{
		std::string packed;

		packed.append(1, local::BeginByte);	// Start of telemetry pack

		packed.append(PackChunk(m_sourceId));
		packed.append(PackChunk(m_destinationId));
		packed.append(PackChunk(static_cast<std::uint32_t>(m_command)));
		packed.append(PackChunk(static_cast<std::uint32_t>(m_phase)));
		packed.append(PackChunk(static_cast<std::uint32_t>(m_nonce)));
		packed.append(PackChunk(static_cast<std::uint32_t>(m_data.size())));
		packed.append(PackChunk(m_data));
		packed.append(PackChunk(NodeUtils::TimePointToString(m_timepoint)));

		packed.append(1, local::EndByte);	// End of telemetry pack

		auto const optSignature = Hmac(packed , packed.size());
		if (optSignature) {
			m_token = *optSignature;
		}
		m_raw = packed;
	}

	//------------------------------------------------------------------------------------------------

	//------------------------------------------------------------------------------------------------
	// Description: Unpack the raw message string into the Message class variables.
	//------------------------------------------------------------------------------------------------
	void Unpack(std::string const& raw)
	{
		std::int32_t loops = 0;
		std::size_t position = 1;
		MessageChunk chunk = MessageChunk::FIRST;

		// Iterate while there are message chunks to be parsed.
		while (chunk <= MessageChunk::LAST) {
			std::size_t end = raw.find(local::SeperatorByte, position);     // Find chunk seperator

			switch (chunk) {
				case MessageChunk::SOURCEID: {
					m_sourceId = raw.substr(position, end - position);
				} break;
				case MessageChunk::DESTINATIONID: {
					m_destinationId = raw.substr(position, end - position);
				} break;
				case MessageChunk::COMMAND: {
					m_command = static_cast<NodeUtils::CommandType>(std::stoi(raw.substr(position, end - position)));
				} break;
				case MessageChunk::PHASE: {
					m_phase = std::stoi(raw.substr(position, end - position));
				} break;
				case MessageChunk::NONCE: {
					m_nonce = std::stoi(raw.substr(position, end - position));
				} break;
				case MessageChunk::DATASIZE: {
					m_length = static_cast<std::int32_t>(std::stoi(raw.substr(position, end - position)));
				} break;
				case MessageChunk::DATA: {
					m_data = raw.substr(position, m_length );
					auto const optData = Decrypt(m_data, m_data.size());
					if (optData) {
						m_data = *optData;
					}
				} break;
				case MessageChunk::TIMESTAMP: {
					m_timepoint = NodeUtils::StringToTimePoint(raw.substr(position, end - position));
				} break;
				default: break;
			}

			position = end + 1;
			chunk = static_cast<MessageChunk>(++loops);
		}

		m_token = raw.substr(position);
		m_raw = raw.substr(0, ( raw.size() - m_token.size()));

		if (std::size_t const found = m_sourceId.find(NodeUtils::ID_SEPERATOR); found != std::string::npos) {
			m_awaitId = m_sourceId.substr(found + 1);
			m_sourceId = m_sourceId.substr(0, found);
		}

		if (std::size_t found = m_destinationId.find(NodeUtils::ID_SEPERATOR); found != std::string::npos) {
			m_awaitId = m_destinationId.substr(found + 1);
			m_destinationId = m_destinationId.substr(0, found);
		}

	}

	//------------------------------------------------------------------------------------------------

	//------------------------------------------------------------------------------------------------
	// Description: Encrypt a provided message with AES-CTR-256 and return ciphertext.
	//------------------------------------------------------------------------------------------------
	std::optional<std::string> Encrypt(std::string const& message, std::uint32_t length)
	{
		if (length == 0) {
			return {};
		}
		unsigned char* ciphertext = new unsigned char[length + 1];
		memset(ciphertext, '\0', length + 1);

		unsigned char iv[16];
		memset(iv, 0, 16);
		memcpy(iv, &m_nonce, sizeof(std::uint32_t));

		auto key = reinterpret_cast<unsigned char const*>(m_key.c_str());
		auto plaintext = reinterpret_cast<unsigned char const*>(message.c_str());

		EVP_CIPHER_CTX* ctx;
		std::int32_t encrypted = 0;
		ctx = EVP_CIPHER_CTX_new();
		EVP_EncryptInit_ex(ctx, EVP_aes_256_ctr(), nullptr, key, iv);
		EVP_EncryptUpdate(ctx, ciphertext, &encrypted, plaintext, length);
		if (encrypted == 0) {
			return {};
		}
		m_length = encrypted;

		EVP_EncryptFinal_ex(ctx, ciphertext + m_length, &encrypted);
		EVP_CIPHER_CTX_free(ctx);
		m_length += encrypted;

		std::string const result =  std::string(reinterpret_cast<char*>(ciphertext), m_length);
		delete[] ciphertext;
		return result;
	}

	//------------------------------------------------------------------------------------------------

	//------------------------------------------------------------------------------------------------
	// Description: Decrypt a provided message with AES-CTR-256 and return decrypted text.
	//------------------------------------------------------------------------------------------------
	std::optional<std::string> Decrypt(std::string const& message, std::uint32_t length)
	{
		if (length == 0) {
			return {};
		}

		unsigned char* plaintext = new unsigned char[length + 1];
		memset(plaintext, '\0', length + 1);

		unsigned char iv[16];
		memset(iv, 0, 16);
		memcpy(iv, &m_nonce, sizeof(std::uint32_t));

 		auto key = reinterpret_cast<unsigned char const*>(m_key.c_str());
 		auto ciphertext = reinterpret_cast<unsigned char const*>(message.c_str());

		EVP_CIPHER_CTX* ctx;
		std::int32_t decrypted = 0;
		ctx = EVP_CIPHER_CTX_new();
		EVP_DecryptInit_ex(ctx, EVP_aes_256_ctr(), nullptr, key, iv);
		EVP_DecryptUpdate(ctx, plaintext, &decrypted, ciphertext, length);
		if (decrypted == 0) {
			return {};
		}
		m_length = decrypted;

		EVP_DecryptFinal_ex(ctx, plaintext + m_length, &decrypted);
		EVP_CIPHER_CTX_free(ctx);
		m_length += decrypted;

		std::string const result = std::string(reinterpret_cast<char*>(plaintext), m_length);
		delete[] plaintext;
		return result; 
	}

	//------------------------------------------------------------------------------------------------
	// Description: HMAC Blake2s a provided message and return the authentication token.
	//------------------------------------------------------------------------------------------------
	std::optional<std::string> Hmac(std::string const& message, std::int32_t length) const
	{
		EVP_MD const* md = EVP_get_digestbyname(local::HashMethod.data());
		auto data = reinterpret_cast<unsigned char const*>(message.c_str());
		auto key = reinterpret_cast<unsigned char const*>(m_key.c_str());

		std::uint32_t hashed = 0;
		auto const signature = HMAC(md, key, m_key.size(), data, length, nullptr, &hashed);
		if (hashed == 0) {
			return {};
		}
		return std::string(reinterpret_cast<char*>(signature), hashed);
	}

	//------------------------------------------------------------------------------------------------

	//------------------------------------------------------------------------------------------------
	// Description: Encode a std::string to a Base64 message
	// Source: https://github.com/ReneNyffenegger/cpp-base64/blob/master/base64.cpp#L45
	//------------------------------------------------------------------------------------------------
	std::string Base64Encode(std::string const& message, std::uint32_t length) const
	{
		std::string encoded;
		std::int32_t idx = 0, jdx;
		std::uint8_t char_array_3[3], char_array_4[4];
		auto bytes_to_encode = reinterpret_cast<std::uint8_t const*>(message.c_str());

		while (length--) {
			char_array_3[idx++] = *(bytes_to_encode++);

			if (idx == 3) {
				char_array_4[0] = (char_array_3[0] & 0xfc) >> 2;
				char_array_4[1] = ((char_array_3[0] & 0x03) << 4) + ((char_array_3[1] & 0xf0) >> 4);
				char_array_4[2] = ((char_array_3[1] & 0x0f) << 2) + ((char_array_3[2] & 0xc0) >> 6);
				char_array_4[3] = char_array_3[2] & 0x3f;

				for (idx = 0; idx < 4; ++idx) {
					encoded += Base64::Characters[char_array_4[idx]];
				}

				idx = 0;
			}
		}

		if (idx) {
			for (jdx = idx; jdx < 3; ++jdx) {
				char_array_3[jdx] = '\0';
			}

			char_array_4[0] = (char_array_3[0] & 0xfc) >> 2;
			char_array_4[1] = ((char_array_3[0] & 0x03) << 4 ) + ((char_array_3[1] & 0xf0) >> 4);
			char_array_4[2] = ((char_array_3[1] & 0x0f) << 2 ) + ((char_array_3[2] & 0xc0) >> 6);

			for (jdx = 0; jdx < idx + 1; ++jdx) {
				encoded += Base64::Characters[char_array_4[jdx]];
			}

			while (idx++ < 3) {
				encoded += '=';
			}
		}

		return encoded;
	}

	//------------------------------------------------------------------------------------------------

	//------------------------------------------------------------------------------------------------
	// Description: Decode a Base64 message to a std::string
	// Source: https://github.com/ReneNyffenegger/cpp-base64/blob/master/base64.cpp#L87
	//------------------------------------------------------------------------------------------------
	std::string Base64Decode(std::string const& message, std::uint32_t length) const
	{
		std::string decoded;
		std::int32_t idx = 0, in_ = 0, jdx; 
		std::uint8_t char_array_3[3], char_array_4[4];

		while (length-- && (message[in_] != '=') && Base64::IsValid(message[in_])) {
			char_array_4[idx++] = message[in_++];

			if (idx == 4 ) {
				for (idx = 0; idx < 4; ++idx) {
					char_array_4[idx] = Base64::Characters.find(char_array_4[idx]);
				}

				char_array_3[0] = (char_array_4[0] << 2) + ((char_array_4[1] & 0x30) >> 4);
				char_array_3[1] = ((char_array_4[1] & 0x0f) << 4) + ((char_array_4[2] & 0x3c) >> 2);
				char_array_3[2] = ((char_array_4[2] & 0x03) << 6) + char_array_4[3];

				for (idx = 0; idx < 3; ++idx) {
					decoded += char_array_3[idx];
				}

				idx = 0;
			}
		}

		if (idx) {
			for (jdx = 0; jdx < idx; ++jdx) {
				char_array_4[jdx] = Base64::Characters.find(char_array_4[jdx]);
			}

			char_array_3[0] = (char_array_4[0] << 2) + ((char_array_4[1] & 0x30) >> 4);
			char_array_3[1] = ((char_array_4[1] & 0x0f) << 4) + ((char_array_4[2] & 0x3c) >> 2);

			for (jdx = 0; jdx < idx - 1; ++jdx) {
				decoded += char_array_3[jdx];
			}
		}

		return decoded;
	}

	//------------------------------------------------------------------------------------------------

	//------------------------------------------------------------------------------------------------
	// Description: Compare the Message token with the computed HMAC.
	//------------------------------------------------------------------------------------------------
	bool Verify() const
	{
		if (!m_raw.empty() || !m_token.empty()) {
			return false;
		}

		auto const verificationToken = Hmac( m_raw, m_raw.size() );
		if (!verificationToken) {
			return false;
		}

		if (m_token != *verificationToken) {
			return false;
		}

		return true;
	}

	//------------------------------------------------------------------------------------------------

private:
	// Mutable to ensure the raw is always reflective of the data contained
	mutable std::string m_raw;	// Raw string format of the message

	std::string m_sourceId;	// ID of the sending node
	std::string m_destinationId;	// ID of the receiving node
	std::optional<NodeUtils::ObjectIdType> m_awaitId;	// ID of the awaiting request on a passdown message

	NodeUtils::CommandType m_command;	// Command type to be run
	std::uint32_t m_phase;	// Phase of the Command state

	std::string m_data;	// Encrypted data to be sent
	std::uint32_t m_length;	 // Data length

	NodeUtils::TimePoint m_timepoint;	// The timepoint that message was created

	NodeUtils::NetworkKey m_key;	// Key used for crypto
	NodeUtils::NetworkNonce m_nonce;	// Current message nonce

	std::unique_ptr<CMessage> m_response;	// A circular message for the response to the current message

	// Mutable to ensure the token is always reflective of the data contained
	mutable std::string m_token;	// Current authentication token created via HMAC

	enum class MessageChunk {
		SOURCEID, DESTINATIONID, COMMAND, PHASE, NONCE, DATASIZE, DATA, TIMESTAMP,
		FIRST = SOURCEID,
		LAST = TIMESTAMP
	};
};

//------------------------------------------------------------------------------------------------
