//------------------------------------------------------------------------------------------------
// File: Message.hpp
// Description:
//------------------------------------------------------------------------------------------------
#pragma once
//------------------------------------------------------------------------------------------------
#include "MessageTypes.hpp"
#include "NodeUtils.hpp"
//------------------------------------------------------------------------------------------------
#include <zmq.h>
#include <cmath>
#include <cstring>
#include <algorithm>
#include <chrono>
#include <functional>
#include <optional>
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
constexpr std::double_t Z85Multiplier = 1.25;
constexpr std::uint32_t TokenSize = 32;

constexpr std::string_view HashMethod = "blake2s256";
//------------------------------------------------------------------------------------------------
} // local namespace
//------------------------------------------------------------------------------------------------
} // namespace
//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------

class CMessage {
public:
	CMessage(
		NodeUtils::NodeIdType const& sourceId,
		NodeUtils::NodeIdType const& destinationId,
		NodeUtils::CommandType command,
		std::int8_t phase,
		std::string_view data,
		NodeUtils::NetworkNonce nonce,
		std::optional<Message::BoundAwaitId> const& awaitId = {})
		: m_raw(std::string())
		, m_sourceId(sourceId)
		, m_destinationId(destinationId)
		, m_boundAwaitId(awaitId)
		, m_command(command)
		, m_phase(phase)
		, m_data()
		, m_key(NodeUtils::NETWORK_KEY)
		, m_nonce(nonce)
		, m_timepoint(NodeUtils::GetSystemTimePoint())
		, m_end(0)
		, m_token()
	{
        Message::Buffer buffer(data.begin(), data.end());
		auto const optData = Encrypt(buffer, data.size());
		if(optData) {
			m_data = *optData;
		}
	}

	//------------------------------------------------------------------------------------------------

	explicit CMessage(std::string_view raw)
		: m_raw(raw)
		, m_sourceId(0)
		, m_destinationId(0)
		, m_boundAwaitId({})
		, m_command(NodeUtils::CommandType::NONE)
		, m_phase(0)
		, m_data()
		, m_key(NodeUtils::NETWORK_KEY)
		, m_nonce(0)
		, m_timepoint(NodeUtils::GetSystemTimePoint())
		, m_end(0)
		, m_token()
	{
		Unpack(Z85Decode(raw));
	}

	//------------------------------------------------------------------------------------------------

	CMessage(CMessage const& other)
		: m_raw(other.m_raw)
		, m_sourceId(other.m_sourceId)
		, m_destinationId(other.m_destinationId)
		, m_boundAwaitId(other.m_boundAwaitId)
		, m_command(other.m_command)
		, m_phase(other.m_phase)
		, m_data(other.m_data)
		, m_key(other.m_key)
		, m_nonce(other.m_nonce)
		, m_timepoint(other.m_timepoint)
		, m_end(other.m_end)
		, m_token(other.m_token)
	{
	}

	//------------------------------------------------------------------------------------------------

	~CMessage() = default;

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

	std::optional<NodeUtils::ObjectIdType> GetAwaitId() const
	{
		if (!m_boundAwaitId) {
			return {};
		}
		return m_boundAwaitId->second;
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
		if (m_raw.empty()){
			Pack();
		}
        return m_raw;
	}


	//------------------------------------------------------------------------------------------------
	// Description: Insert a chunk of data into the message buffer
	//------------------------------------------------------------------------------------------------
    template<typename ChunkType>
	void PackChunk(Message::Buffer& buffer, ChunkType const& chunk) const
	{
		auto const begin = reinterpret_cast<std::uint8_t const*>(&chunk);
        auto const end = begin + sizeof(chunk);
        buffer.insert(buffer.end(), begin, end);
	}

	//------------------------------------------------------------------------------------------------

    // template<>
	void PackChunk(Message::Buffer& buffer, Message::Buffer const& chunk) const
	{
        buffer.insert(buffer.end(), chunk.begin(), chunk.end());
	}

	//------------------------------------------------------------------------------------------------

	//------------------------------------------------------------------------------------------------
	// Description: Pack the Message class values into a single raw string.
	//------------------------------------------------------------------------------------------------
	void Pack() const
	{
        Message::Buffer buffer;
        buffer.reserve(FixedPackBufferSize() + m_data.size());

		PackChunk(buffer, m_sourceId);
		PackChunk(buffer, m_destinationId);
		if (m_boundAwaitId) {
			PackChunk(buffer, m_boundAwaitId->first);
			PackChunk(buffer, m_boundAwaitId->second);
		} else {
			PackChunk(buffer, Message::AwaitBinding::NONE);
		}
		PackChunk(buffer, m_command);
		PackChunk(buffer, m_phase);
		PackChunk(buffer, m_nonce);
		PackChunk(buffer, static_cast<std::uint16_t>(m_data.size()));
		PackChunk(buffer, m_data);
		PackChunk(buffer, NodeUtils::TimePointToTimePeriod(m_timepoint));
		m_end = buffer.size();

        m_raw.clear();
		auto const optSignature = Hmac(buffer, buffer.size());
		if (optSignature) {
			m_token = *optSignature;
            buffer.insert(buffer.end(), m_token.begin(), m_token.end());
			m_raw = Z85Encode(buffer);
		}
	}

	//------------------------------------------------------------------------------------------------
	
	template<typename ChunkType>
	void UnpackChunk(Message::Buffer const& buffer, std::uint32_t& position, ChunkType& destination)
	{
		std::uint32_t size = sizeof(destination);
		std::memcpy(&destination, buffer.data() + position, size);
		position += size;
	}

	//------------------------------------------------------------------------------------------------

	void UnpackChunk(
		Message::Buffer const& buffer,
		std::uint32_t& position,
		Message::Buffer& destination,
		std::uint32_t const& size)
	{
		Message::Buffer::const_iterator begin = buffer.begin() + position;
		Message::Buffer::const_iterator end = begin + size;
		destination.insert(destination.begin(), begin, end);
		position += size;
	}

	//------------------------------------------------------------------------------------------------

	//------------------------------------------------------------------------------------------------
	// Description: Unpack the raw message string into the Message class variables.
	//------------------------------------------------------------------------------------------------
	void Unpack(Message::Buffer const& buffer)
	{
		std::uint32_t position = 0;

		UnpackChunk(buffer, position, m_sourceId);
		UnpackChunk(buffer, position, m_destinationId);

		Message::AwaitBinding awaitBinding = Message::AwaitBinding::NONE;
		UnpackChunk(buffer, position, awaitBinding);
		if (awaitBinding != Message::AwaitBinding::NONE) {
			NodeUtils::ObjectIdType awaitId = 0;
			UnpackChunk(buffer, position, awaitId);
			m_boundAwaitId = Message::BoundAwaitId({awaitBinding, awaitId});
		}
		
		UnpackChunk(buffer, position, m_command);
		UnpackChunk(buffer, position, m_phase);
		UnpackChunk(buffer, position, m_nonce);

		std::uint16_t dataLength = 0;
		UnpackChunk(buffer, position, dataLength);
		UnpackChunk(buffer, position, m_data, dataLength);

		std::uint64_t timestamp;
		UnpackChunk(buffer, position, timestamp);
		m_timepoint = NodeUtils::TimePoint(NodeUtils::TimePeriod(timestamp));
		m_end = position;

		UnpackChunk(buffer, position, m_token, local::TokenSize);
	}

	//------------------------------------------------------------------------------------------------

	//------------------------------------------------------------------------------------------------
	// Description: Encrypt a provided message with AES-CTR-256 and return ciphertext.
	//------------------------------------------------------------------------------------------------
	std::optional<Message::Buffer> Encrypt(Message::Buffer const& message, std::uint32_t length) const
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

		std::uint32_t adjustedLength = encrypted;
		EVP_EncryptFinal_ex(ctx, ciphertext + encrypted, &encrypted);
		EVP_CIPHER_CTX_free(ctx);
		adjustedLength += encrypted;

        auto const begin = reinterpret_cast<std::uint8_t const*>(ciphertext);
        auto const end = begin + adjustedLength;
		Message::Buffer buffer(begin, end);
		delete[] ciphertext;

		return buffer;
	}

	//------------------------------------------------------------------------------------------------

	//------------------------------------------------------------------------------------------------
	// Description: Decrypt a provided message with AES-CTR-256 and return decrypted text.
	//------------------------------------------------------------------------------------------------
	std::optional<Message::Buffer> Decrypt(Message::Buffer const& message, std::uint32_t length) const
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

		std::uint32_t adjustedLength = decrypted;
		EVP_DecryptFinal_ex(ctx, plaintext + decrypted, &decrypted);
		EVP_CIPHER_CTX_free(ctx);
		adjustedLength += decrypted;

        auto const begin = reinterpret_cast<std::uint8_t const*>(plaintext);
        auto const end = begin + adjustedLength;
		Message::Buffer buffer(begin, end);
		delete[] plaintext;

		return buffer;
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

		return Message::Buffer(&signature[0], &signature[hashed]);
	}

	//------------------------------------------------------------------------------------------------

	//------------------------------------------------------------------------------------------------
	// Description: Compare the Message token with the computed HMAC.
	//------------------------------------------------------------------------------------------------
	Message::VerificationStatus Verify() const
	{
		if (m_raw.empty() || m_token.empty()) {
			return Message::VerificationStatus::UNAUTHORIZED;
		}

        Message::Buffer buffer = Z85Decode(m_raw);
		Message::Buffer base(buffer.begin(), buffer.begin() + m_end);

		auto const token = Hmac(base, base.size());
		if (!token || m_token != *token) {
			return Message::VerificationStatus::UNAUTHORIZED;
		}

		return Message::VerificationStatus::SUCCESS;
	}

	//------------------------------------------------------------------------------------------------

    //------------------------------------------------------------------------------------------------
    // Description: Encode a Message::Buffer to a Z85 message
	// Warning: The source buffer may increase in size to be a multiple of 4. Z85 needs to be this 
	// padding.
    //------------------------------------------------------------------------------------------------
    inline std::string Z85Encode(Message::Buffer& message) const
    {
		std::string encoded;
		// Pad the buffer such that it's length is a multiple of 4 for z85 encoding
		for (std::size_t idx = 0; idx < (message.size() % 4); ++idx) {
			message.emplace_back(0);
		}

		// Resize m_raw to be at least as large as the maximum encoding length of z85
		// z85 will expand the byte length to 25% more than the input length
		encoded.resize(ceil(message.size() * local::Z85Multiplier));
		zmq_z85_encode(encoded.data(), message.data(), message.size());
		return encoded;
    }

    //------------------------------------------------------------------------------------------------

    //------------------------------------------------------------------------------------------------
    // Description: Decode a Z85 message to a std::string
    //------------------------------------------------------------------------------------------------
    inline Message::Buffer Z85Decode(std::string_view message) const
    {
        Message::Buffer decoded;
		// Resize the destination to be the estimated maximum size
		decoded.resize(message.size() / local::Z85Multiplier);
		zmq_z85_decode(decoded.data(), message.data());
        return decoded;
    }

    //------------------------------------------------------------------------------------------------

private:
	constexpr std::size_t FixedPackBufferSize() const
	{
		std::size_t size = 0;
		size += sizeof(m_sourceId);
		size += sizeof(m_destinationId);
		size += sizeof(m_boundAwaitId->first);
		size += sizeof(m_boundAwaitId->second);
		size += sizeof(m_command);
		size += sizeof(m_phase);
		size += sizeof(std::uint16_t);
		size += sizeof(m_nonce);
		size += sizeof(std::uint64_t);
		size += local::TokenSize;
		return size;
	}

	// Mutable to ensure the raw is always reflective of the data contained
	mutable std::string m_raw;	// Raw encoded payload of the message

	NodeUtils::NodeIdType m_sourceId;	// ID of the sending node
	NodeUtils::NodeIdType m_destinationId;	// ID of the receiving node
	std::optional<Message::BoundAwaitId> m_boundAwaitId;	// ID bound to the source or destination on a passdown message

	NodeUtils::CommandType m_command;	// Command type to be run
	std::uint8_t m_phase;	// Phase of the Command state

	Message::Buffer m_data;	// Primary message content

	NodeUtils::NetworkKey m_key;	// Key used for encryption and authentication
	NodeUtils::NetworkNonce m_nonce;	// Current message nonce

	NodeUtils::TimePoint m_timepoint;	// The timepoint that message was created
	mutable std::uint32_t m_end; // Ending position of the message payload when packed into a buffer

	// Mutable to ensure the token is always reflective of the data contained
	mutable Message::Token m_token;	// Current authentication token created via HMAC
};

//------------------------------------------------------------------------------------------------

