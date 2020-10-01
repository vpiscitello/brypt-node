//------------------------------------------------------------------------------------------------
// File: MessageSecurity.cpp
// Description:
//------------------------------------------------------------------------------------------------
#include "ApplicationMessage.hpp"
#include "MessageSecurity.hpp"
#include "PackUtils.hpp"
#include "../Utilities/NodeUtils.hpp"
//------------------------------------------------------------------------------------------------
#include <array>
#include <cstdint>
#include <memory>
#include <optional>
#include <vector>
//------------------------------------------------------------------------------------------------
#include <openssl/conf.h>
#include <openssl/crypto.h>
#include <openssl/err.h>
#include <openssl/sha.h>
#include <openssl/evp.h>
#include <openssl/hmac.h>
//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
namespace {
namespace local {
//------------------------------------------------------------------------------------------------

using CipherContext = std::unique_ptr<EVP_CIPHER_CTX, decltype(&EVP_CIPHER_CTX_free)>;

//------------------------------------------------------------------------------------------------
} // local namespace
} // namespace
//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
// Description: Encrypt a provided message with AES-CTR-256 and return ciphertext.
//------------------------------------------------------------------------------------------------
std::optional<Message::Buffer> MessageSecurity::Encrypt(
    Message::Buffer const& buffer, std::uint32_t length, std::uint64_t nonce)
{
	if (length == 0) {
		return {};
	}

	local::CipherContext upCipherContext(EVP_CIPHER_CTX_new(), &EVP_CIPHER_CTX_free);
	if (ERR_get_error() != 0 || upCipherContext == nullptr) {
		return {};
	}

	auto const pEncryptionKey = reinterpret_cast<std::uint8_t const*>(NodeUtils::NetworkKey.data());
	std::array<std::uint8_t, 16> iv = {};
	std::memcpy(iv.data(), &nonce, sizeof(nonce));

	EVP_EncryptInit_ex(upCipherContext.get(), EVP_aes_256_ctr(), nullptr, pEncryptionKey, iv.data());
	if (ERR_get_error() != 0) {
		return {};
	}

	Message::Buffer ciphertext(length, 0);
	auto pCiphertext = reinterpret_cast<std::uint8_t*>(ciphertext.data());
	auto const pPlaintext = reinterpret_cast<std::uint8_t const*>(buffer.data());

	std::int32_t encrypted = 0;
	EVP_EncryptUpdate(upCipherContext.get(), pCiphertext, &encrypted, pPlaintext, length);
	if (ERR_get_error() != 0 || encrypted == 0) {
		return {};
	}

	EVP_EncryptFinal_ex(upCipherContext.get(), pCiphertext + encrypted ,&encrypted);
	if (ERR_get_error() != 0) {
		return {};
	}

	return ciphertext;
}

//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
// Description: Decrypt a provided message with AES-CTR-256 and return decrypted text.
//------------------------------------------------------------------------------------------------
std::optional<Message::Buffer> MessageSecurity::Decrypt(
    Message::Buffer const& buffer, std::uint32_t length, std::uint64_t nonce)
{
	if (length == 0) {
		return {};
	}

	local::CipherContext upCipherContext(EVP_CIPHER_CTX_new(), &EVP_CIPHER_CTX_free);
	if (ERR_get_error() != 0 || upCipherContext == nullptr) {
		return {};
	}

	auto const pEncryptionKey = reinterpret_cast<std::uint8_t const*>(NodeUtils::NetworkKey.data());
	std::array<std::uint8_t, 16> iv = {};
	std::memcpy(iv.data(), &nonce, sizeof(nonce));

	EVP_DecryptInit_ex(upCipherContext.get(), EVP_aes_256_ctr(), nullptr, pEncryptionKey, iv.data());
	if (ERR_get_error() != 0) {
		return {};
	}

	Message::Buffer plaintext(length, 0);
	auto pPlaintext = reinterpret_cast<std::uint8_t*>(plaintext.data());
	auto const pCiphertext = reinterpret_cast<std::uint8_t const*>(buffer.data());

	std::int32_t decrypted = 0;
	EVP_DecryptUpdate(upCipherContext.get(), pPlaintext, &decrypted, pCiphertext, length);
	if (ERR_get_error() != 0 || decrypted == 0) {
		return {};
	}

	EVP_DecryptFinal_ex(upCipherContext.get(), pPlaintext + decrypted ,&decrypted);
	if (ERR_get_error() != 0) {
		return {};
	}

	return plaintext;
}

//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
// Description: HMAC Blake2s a provided message and return the authentication token.
//------------------------------------------------------------------------------------------------
std::optional<Message::Buffer> MessageSecurity::HMAC(
    Message::Buffer const& buffer,
    std::uint32_t length)
{
	if (length == 0) {
		return {};
	}

	auto data = reinterpret_cast<unsigned char const*>(buffer.data());
	auto key = reinterpret_cast<unsigned char const*>(NodeUtils::NetworkKey.data());

	EVP_MD const* pDigest = EVP_get_digestbyname(HashMethod.data());
	if (ERR_get_error() != 0 || pDigest == nullptr) {
		return {};
	}

	std::uint32_t hashed = 0;
	auto const signature = HMAC(pDigest, key, NodeUtils::NetworkKey.size(), data, length, nullptr, &hashed);
	if (ERR_get_error() != 0 || hashed == 0) {
		return {};
	}

	return Message::Buffer(&signature[0], &signature[hashed]);
}
	
//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
// Description: Compare the Message token with the computed HMAC.
//------------------------------------------------------------------------------------------------
MessageSecurity::VerificationStatus MessageSecurity::Verify(Message::Buffer const& buffer)
{
	if (buffer.empty()) {
		return VerificationStatus::Unauthorized;
	}

    std::uint32_t packContentSize = buffer.size() - TokenSize;
    Message::Buffer attachedTokenBuffer;
    attachedTokenBuffer.reserve(packContentSize);

	Message::Buffer::const_iterator tokenBegin = buffer.end() - TokenSize;
	Message::Buffer::const_iterator tokenEnd = buffer.end();
	PackUtils::UnpackChunk(tokenBegin, tokenEnd, attachedTokenBuffer, TokenSize);
    if (attachedTokenBuffer.empty()) {
		return VerificationStatus::Unauthorized;
    }

	auto const optGeneratedBuffer = MessageSecurity::HMAC(buffer, packContentSize);
	if (!optGeneratedBuffer || optGeneratedBuffer->size() != attachedTokenBuffer.size()) {
		return VerificationStatus::Unauthorized;
	}

	auto const result = CRYPTO_memcmp(
		optGeneratedBuffer->data(),
		attachedTokenBuffer.data(),
		optGeneratedBuffer->size());

	if (result != 0) {
		return VerificationStatus::Unauthorized;
	}

	return VerificationStatus::Success;
}

//------------------------------------------------------------------------------------------------
