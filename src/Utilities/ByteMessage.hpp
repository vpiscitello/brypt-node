//------------------------------------------------------------------------------------------------
// File: Message.hpp
// Description:
//------------------------------------------------------------------------------------------------
#pragma once
//------------------------------------------------------------------------------------------------
#include "MessageTypes.hpp"
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
using NodeIdType = std::uint32_t;
using ObjectIdType = std::uint32_t;
using NetworkKey = std::string_view;

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
		, m_sourceId(0)
		, m_destinationId(0)
		, m_awaitId({})
		, m_command(NodeUtils::CommandType::NONE)
		, m_phase(0)
		, m_data()
		, m_length(0)
		, m_timepoint(NodeUtils::GetSystemTimePoint())
		, m_key(NodeUtils::NETWORK_KEY)
		, m_nonce(0)
		, m_token()
        , m_updated(false)
	{
	}

	//------------------------------------------------------------------------------------------------

	CMessage(
		local::NodeIdType const& sourceId,
		local::NodeIdType const& destinationId,
		NodeUtils::CommandType command,
		std::int8_t phase,
		std::string const& data,
		NodeUtils::NetworkNonce nonce)
		: m_raw(std::string())
		, m_sourceId(sourceId)
		, m_destinationId(destinationId)
		, m_awaitId({})
		, m_command(command)
		, m_phase(phase)
		, m_data()
		, m_length(0)
		, m_timepoint(NodeUtils::GetSystemTimePoint())
		, m_key(NodeUtils::NETWORK_KEY)
		, m_nonce(nonce)
		, m_token()
        , m_updated(true)
	{
        auto const begin = reinterpret_cast<std::uint8_t const*>(data.c_str());
        auto const end = begin + data.size();
        Message::Buffer buffer(begin, end);
		auto const optData = Encrypt(buffer, data.size());
		if(optData) {
			m_data = *optData;
		}
	}

	//------------------------------------------------------------------------------------------------

	explicit CMessage(std::string const& raw)
		: m_raw(raw)
		, m_sourceId(0)
		, m_destinationId(0)
		, m_awaitId({})
		, m_command(NodeUtils::CommandType::NONE)
		, m_phase(0)
		, m_data()
		, m_length(0)
		, m_timepoint(NodeUtils::GetSystemTimePoint())
		, m_key(NodeUtils::NETWORK_KEY)
		, m_nonce(0)
		, m_token()
        , m_updated(false)
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
		, m_token(other.m_token)
        , m_updated(other.m_updated)
	{
	}

	//------------------------------------------------------------------------------------------------

	~CMessage()
	{
	}

	//------------------------------------------------------------------------------------------------

	local::NodeIdType const& GetSourceId() const
	{
		return m_sourceId;
	}

	//------------------------------------------------------------------------------------------------

	local::NodeIdType const& GetDestinationId() const
	{
		return m_destinationId;
	}

	//------------------------------------------------------------------------------------------------

	std::optional<local::ObjectIdType> const& GetAwaitId() const
	{
		return m_awaitId;
	}

	//------------------------------------------------------------------------------------------------

	NodeUtils::CommandType GetCommand() const
	{
		return m_command;
	}

	//------------------------------------------------------------------------------------------------

	std::uint32_t GetPhase() const
	{
		return m_phase;
	}

	//------------------------------------------------------------------------------------------------

	Message::Buffer const& GetData() const
	{
		return m_data;
	}

	//------------------------------------------------------------------------------------------------

	NodeUtils::TimePoint const& GetSystemTimePoint() const
	{
		return m_timepoint;
	}

	//------------------------------------------------------------------------------------------------

	NodeUtils::NetworkNonce GetNonce() const
	{
		return m_nonce;
	}

	//------------------------------------------------------------------------------------------------

	std::string const& GetPack() const
	{
		if (m_updated || m_raw.empty()){
			Pack();
		}
        return m_raw;
	}

	//------------------------------------------------------------------------------------------------

	void SetSourceId(local::NodeIdType const& sourceId)
	{
        m_updated = true;
		m_sourceId = sourceId;
	}

	//------------------------------------------------------------------------------------------------

	void SetDestinationId(local::NodeIdType const& destinationId)
	{
        m_updated = true;
		m_destinationId = destinationId;
	}

	//------------------------------------------------------------------------------------------------

	void SetCommand(NodeUtils::CommandType command, std::uint8_t phase)
	{
        m_updated = true;
		m_command = command;
		m_phase = phase;
	}

	//------------------------------------------------------------------------------------------------

	void SetData(Message::Buffer const& data)
	{
        m_updated = true;
		m_data = data;
	}

	//------------------------------------------------------------------------------------------------

	void SetNonce(NodeUtils::NetworkNonce nonce)
	{
        m_updated = true;
		m_nonce = nonce;
	}

	//------------------------------------------------------------------------------------------------

	void SetTimestamp()
	{
        m_updated = false;
		m_timepoint = NodeUtils::GetSystemTimePoint();
	}

	//------------------------------------------------------------------------------------------------

	//------------------------------------------------------------------------------------------------
	// Description: Wrap a string message chunk with seperators.
	//------------------------------------------------------------------------------------------------
    template<typename ChunkType>
	void PackChunk(Message::Buffer& buffer, ChunkType const& chunk) const
	{
		auto const begin = reinterpret_cast<std::uint8_t const*>(&chunk);
        auto const end = begin + sizeof(chunk);
        buffer.insert(buffer.end(), begin, end);
		buffer.emplace_back(local::SeperatorByte);	// Telemetry seperator
	}

	//------------------------------------------------------------------------------------------------

	//------------------------------------------------------------------------------------------------
	// Description: Pack the Message class values into a single raw string.
	//------------------------------------------------------------------------------------------------
	void Pack() const
	{
        Message::Buffer buffer;
        buffer.reserve((512 / sizeof(std::uint8_t)) + m_data.size());

		buffer.emplace_back(local::BeginByte);	// Start of telemetry pack

		PackChunk(buffer, m_sourceId);
		PackChunk(buffer, m_destinationId);
		PackChunk(buffer, m_command);
		PackChunk(buffer, m_phase);
		PackChunk(buffer, m_nonce);
		PackChunk(buffer, m_data.size());
		PackChunk(buffer, m_data);
		PackChunk(buffer, NodeUtils::TimePointToTimePeriod(m_timepoint));

		buffer.emplace_back(local::EndByte);	// End of telemetry pack

        m_raw.clear();
		auto const optSignature = Hmac(buffer , m_raw.size());
		if (optSignature) {
			Message::Buffer const& token = *optSignature;
            buffer.insert(buffer.end(), token.begin(), token.end());
            m_raw = Base64Encode(buffer, buffer.size());
		}
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
	std::optional<Message::Buffer> Encrypt(Message::Buffer const& message, std::uint32_t length)
	{
		if (length == 0) {
			return {};
		}

		auto plaintext = reinterpret_cast<unsigned char const*>(message.data());
		auto key = reinterpret_cast<unsigned char const*>(m_key.data());
		unsigned char iv[16];
		memset(iv, 0, 16);
		memcpy(iv, &m_nonce, sizeof(m_nonce));

		unsigned char* ciphertext = new unsigned char[length + 1];
		memset(ciphertext, '\0', length + 1);

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
		delete[] ciphertext;

        auto const begin = reinterpret_cast<std::uint8_t const*>(ciphertext);
        auto const end = begin + m_length;
		return Message::Buffer(begin, end);
	}

	//------------------------------------------------------------------------------------------------

	//------------------------------------------------------------------------------------------------
	// Description: Decrypt a provided message with AES-CTR-256 and return decrypted text.
	//------------------------------------------------------------------------------------------------
	std::optional<Message::Buffer> Decrypt(Message::Buffer const& message, std::uint32_t length)
	{
		if (length == 0) {
			return {};
		}

 		auto ciphertext = reinterpret_cast<unsigned char const*>(message.data());
 		auto key = reinterpret_cast<unsigned char const*>(m_key.data());
		unsigned char iv[16];
		memset(iv, 0, 16);
		memcpy(iv, &m_nonce, sizeof(std::uint32_t));

		unsigned char* plaintext = new unsigned char[length + 1];
		memset(plaintext, '\0', length + 1);

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
		delete[] plaintext;

        auto const begin = reinterpret_cast<std::uint8_t const*>(plaintext);
        auto const end = begin + m_length;
		return Message::Buffer(begin, end);
	}

	//------------------------------------------------------------------------------------------------
	// Description: HMAC Blake2s a provided message and return the authentication token.
	//------------------------------------------------------------------------------------------------
	std::optional<Message::Buffer> Hmac(Message::Buffer const& message, std::uint32_t length) const
	{
        if (length == 0) {
			return {};
		}

		auto data = reinterpret_cast<unsigned char const*>(message.data());
		auto key = reinterpret_cast<unsigned char const*>(m_key.data());

		EVP_MD const* md = EVP_get_digestbyname(local::HashMethod.data());
		std::uint32_t hashed = 0;
		auto const signature = HMAC(md, key, m_key.size(), data, length, nullptr, &hashed);
		if (hashed == 0) {
			return {};
		}
        auto const begin = reinterpret_cast<std::uint8_t const*>(signature);
        auto const end = begin + m_length;
		return Message::Buffer(begin, end);
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

        Message::Buffer buffer = Base64Decode(m_raw, m_raw.size());
		auto const verificationToken = Hmac(buffer, buffer.size());
		if (!verificationToken) {
			return false;
		}

		if (m_token != *verificationToken) {
			return false;
		}

		return true;
	}

	//------------------------------------------------------------------------------------------------

    //------------------------------------------------------------------------------------------------
    // Description: Encode a std::string to a Base64 message
    // Source: https://github.com/ReneNyffenegger/cpp-base64/blob/master/base64.cpp#L45
    //------------------------------------------------------------------------------------------------
    inline std::string Base64Encode(Message::Buffer const& message, std::uint32_t length) const
    {
        std::string encoded;
        std::int32_t idx = 0, jdx;
        std::uint8_t char_array_3[3], char_array_4[4];
        auto bytes_to_encode = reinterpret_cast<std::uint8_t const*>(message.data());

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
    inline Message::Buffer Base64Decode(std::string const& message, std::uint32_t length) const
    {
        Message::Buffer decoded;
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
                    decoded.emplace_back(char_array_3[idx]);
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
				decoded.emplace_back(char_array_3[idx]);
            }
        }

        return decoded;
    }

    //------------------------------------------------------------------------------------------------

private:
	// Mutable to ensure the raw is always reflective of the data contained
	mutable std::string m_raw;	// Raw string format of the message

	local::NodeIdType m_sourceId;	// ID of the sending node
	local::NodeIdType m_destinationId;	// ID of the receiving node
	std::optional<local::ObjectIdType> m_awaitId;	// ID of the awaiting request on a passdown message

	NodeUtils::CommandType m_command;	// Command type to be run
	std::uint32_t m_phase;	// Phase of the Command state

	Message::Buffer m_data;	// Encrypted data to be sent
	std::uint16_t m_length;	 // Data length

	NodeUtils::TimePoint m_timepoint;	// The timepoint that message was created

	local::NetworkKey m_key;	// Key used for crypto
	NodeUtils::NetworkNonce m_nonce;	// Current message nonce

	// Mutable to ensure the token is always reflective of the data contained
	mutable Message::Token m_token;	// Current authentication token created via HMAC
    bool m_updated; // Dirty bit to generate a new token if data has been updated
};

//------------------------------------------------------------------------------------------------

