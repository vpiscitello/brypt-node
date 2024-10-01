//----------------------------------------------------------------------------------------------------------------------
// File: KeyStore.hpp
// Description: The KeyStore is a container for the various public and shared keys 
// required by a security strategy. The KeyStore will take ownership of any keys provided to it
// in order to manage their lifetime. It is expected that the KeyStore is provided a shared 
// secret produced through key exchange or key encapsulation and will derive content and signature
// secrets from the data. The derived keys will be partitions of the principal derived key, 
// Derived key interfaces (e.g. GetContentKey) will provide the start of the key cordon and 
// the number of bytes in the cordon range. Currently, the key key derivation method is fixed 
// for all security strategies and uses SHA3-256.  At destruction time shared secrets will be 
// cleared and zeroed in memory. 
//----------------------------------------------------------------------------------------------------------------------
#pragma once
//----------------------------------------------------------------------------------------------------------------------
#include "SecurityTypes.hpp"
#include "SecureBuffer.hpp"
//----------------------------------------------------------------------------------------------------------------------
#include <algorithm>
#include <cstdint>
#include <memory>
#include <optional>
#include <ranges>
#include <utility>
//----------------------------------------------------------------------------------------------------------------------

typedef struct evp_kdf_st EVP_KDF;
typedef struct evp_md_st EVP_MD;

extern "C" void EVP_KDF_free(EVP_KDF *kdf);

//----------------------------------------------------------------------------------------------------------------------
namespace Security {
//----------------------------------------------------------------------------------------------------------------------

class CipherSuite;

class KeyStore;

class PublicKey;
class Salt;
class SharedSecret;
class SupplementalData;
class PrincipalKey;
class EncryptionKey;
class SignatureKey;

using OptionalEncryptionKey = std::optional<EncryptionKey>;
using OptionalPublicKey = std::optional<PublicKey>;
using OptionalPrincipalKey = std::optional<PrincipalKey>;
using OptionalSignatureKey = std::optional<SignatureKey>;

//----------------------------------------------------------------------------------------------------------------------
} // Security namespace
//----------------------------------------------------------------------------------------------------------------------

class Security::PublicKey
{
public:
    PublicKey() = default;
    PublicKey(Buffer&& data) : m_data(std::move(data)) {}
    PublicKey(ReadableView data) : m_data(data) {}
    
    PublicKey(PublicKey const& other) : m_data(other.m_data) {}

    PublicKey& operator=(PublicKey const& other) {
        m_data = other.m_data;
        return *this;
    }

    PublicKey(PublicKey&& other) noexcept : m_data(std::exchange(other.m_data, {})) {}

    PublicKey& operator=(PublicKey&& other) noexcept {
        if (this != &other) {
            m_data = std::exchange(other.m_data, {});
        }
        return *this;
    }

    [[nodiscard]] bool operator==(PublicKey const& other) const noexcept = default;
    [[nodiscard]] std::strong_ordering operator<=>(PublicKey const& other) const noexcept = default;

    template<typename T>
    auto Read(std::function<T(Buffer const&)> const& reader) const -> decltype(auto) { return m_data.Read(reader); }

    [[nodiscard]] ReadableView GetData() const { return m_data.GetData(); }
    [[nodiscard]] WriteableView GetData() { return m_data.GetData(); }
    [[nodiscard]] std::size_t GetSize() const { return m_data.GetSize(); }
    [[nodiscard]] bool IsEmpty() const { return m_data.IsEmpty(); }
    void Erase() { m_data.Erase(); }

private:
    SecureBuffer m_data;
};

//----------------------------------------------------------------------------------------------------------------------

class Security::Salt
{
public:
    Salt() = default;
    Salt(Buffer&& data) : m_data(std::move(data)) {}
    Salt(ReadableView data) : m_data(data) {}

    Salt(Salt const& other) : m_data(other.m_data) {}

    Salt& operator=(Salt const& other) {
        m_data = other.m_data;
        return *this;
    }

    Salt(Salt&& other) noexcept : m_data(std::exchange(other.m_data, {})) {}

    Salt& operator=(Salt&& other) noexcept {
        if (this != &other) {
            m_data = std::exchange(other.m_data, {});
        }
        return *this;
    }

    [[nodiscard]] bool operator==(Salt const& other) const noexcept = default;
    [[nodiscard]] std::strong_ordering operator<=>(Salt const& other) const noexcept = default;

    [[nodiscard]] ReadableView GetData() const { return m_data.GetData(); }
    [[nodiscard]] WriteableView GetData() { return m_data.GetData(); }
    [[nodiscard]] std::size_t GetSize() const { return m_data.GetSize(); }
    [[nodiscard]] bool IsEmpty() const { return m_data.IsEmpty(); }

    void Append(ReadableView data) { m_data.Append(data); }
    void Erase() { m_data.Erase(); }

private:
    SecureBuffer m_data;
};

//----------------------------------------------------------------------------------------------------------------------

class Security::SharedSecret
{
public:
    SharedSecret(Buffer&& data) : m_data(std::move(data)) {}

    SharedSecret(SharedSecret const&) = delete;
    SharedSecret& operator=(SharedSecret const&) = delete;

    SharedSecret(SharedSecret&& other) noexcept : m_data(std::exchange(other.m_data, {})) {}

    SharedSecret& operator=(SharedSecret&& other) noexcept {
        if (this != &other) {
            m_data = std::exchange(other.m_data, {});
        }
        return *this;
    }

    [[nodiscard]] bool operator==(SharedSecret const& other) const noexcept = default;
    [[nodiscard]] std::strong_ordering operator<=>(SharedSecret const& other) const noexcept = default;

    [[nodiscard]] ReadableView GetData() const { return m_data.GetData(); }
    [[nodiscard]] WriteableView GetData() { return m_data.GetData(); }
    [[nodiscard]] std::size_t GetSize() const { return m_data.GetSize(); }
    [[nodiscard]] bool IsEmpty() const { return m_data.IsEmpty(); }

private:
    SecureBuffer m_data;
};

//----------------------------------------------------------------------------------------------------------------------

class Security::SupplementalData
{
public:
    SupplementalData() : m_data() {}
    SupplementalData(Buffer&& data) : m_data(std::move(data)) {}

    SupplementalData(SupplementalData const&) = delete;
    SupplementalData& operator=(SupplementalData const&) = delete;

    SupplementalData(SupplementalData&& other) noexcept : m_data(std::exchange(other.m_data, {})) {}

    SupplementalData& operator=(SupplementalData&& other) noexcept {
        if (this != &other) {
            m_data = std::exchange(other.m_data, {});
        }
        return *this;
    }

    [[nodiscard]] bool operator==(SupplementalData const& other) const noexcept = default;
    [[nodiscard]] std::strong_ordering operator<=>(SupplementalData const& other) const noexcept = default;

    template<typename T>
    auto Read(std::function<T(Buffer const&)> const& reader) const -> decltype(auto) { return m_data.Read(reader); }

    [[nodiscard]] ReadableView GetData() const { return m_data.GetData(); }
    [[nodiscard]] WriteableView GetData() { return m_data.GetData(); }
    [[nodiscard]] std::size_t GetSize() const { return m_data.GetSize(); }
    [[nodiscard]] bool IsEmpty() const { return m_data.IsEmpty(); }

private:
    SecureBuffer m_data;
};

//----------------------------------------------------------------------------------------------------------------------

class Security::PrincipalKey
{
public:
    PrincipalKey(SecureBuffer&& data) : m_data(std::move(data)) {}

    PrincipalKey(PrincipalKey const&) = delete;
    PrincipalKey& operator=(PrincipalKey const&) = delete;

    PrincipalKey(PrincipalKey&& other) noexcept : m_data(std::exchange(other.m_data, {})) {}

    PrincipalKey& operator=(PrincipalKey&& other) noexcept {
        if (this != &other) {
            m_data = std::exchange(other.m_data, {});
        }
        return *this;
    }

    [[nodiscard]] bool operator==(PrincipalKey const& other) const noexcept = default;
    [[nodiscard]] std::strong_ordering operator<=>(PrincipalKey const& other) const noexcept = default;

    [[nodiscard]] ReadableView GetData() const { return m_data.GetData(); }
    [[nodiscard]] WriteableView GetData() { return m_data.GetData(); }
    [[nodiscard]] ReadableView GetCordon(std::size_t offset, std::size_t size) const { return m_data.GetCordon(offset, size); }
    [[nodiscard]] std::size_t GetSize() const { return m_data.GetSize(); }
    [[nodiscard]] bool IsEmpty() const { return m_data.IsEmpty(); }

    void Erase() { m_data.Erase(); }

private:
    SecureBuffer m_data;
};

//----------------------------------------------------------------------------------------------------------------------

class Security::EncryptionKey
{
public:
    EncryptionKey(ReadableView const& cordons) : m_cordons(cordons) {}

    EncryptionKey(EncryptionKey const&) = delete;
    EncryptionKey& operator=(EncryptionKey const&) = delete;

    EncryptionKey(EncryptionKey&& other) noexcept : m_cordons(std::exchange(other.m_cordons, {})) {}

    EncryptionKey& operator=(EncryptionKey&& other) noexcept {
        if (this != &other) {
            m_cordons = std::exchange(other.m_cordons, {});
        }
        return *this;
    }

    [[nodiscard]] bool operator==(EncryptionKey const& other) const noexcept
    {
        return std::ranges::equal(m_cordons, other.m_cordons);
    }

    [[nodiscard]] std::strong_ordering operator<=>(EncryptionKey const& other) const noexcept
    {
        return std::lexicographical_compare_three_way(
            m_cordons.begin(), m_cordons.end(), other.m_cordons.begin(), other.m_cordons.end());
    }

    [[nodiscard]] std::uint8_t const* GetData() const { return m_cordons.data(); }
    [[nodiscard]] std::size_t GetSize() const { return m_cordons.size(); }
    [[nodiscard]] bool IsEmpty() const { return m_cordons.empty(); }

    void Erase() { m_cordons = {}; }

private:
    ReadableView m_cordons;
};

//----------------------------------------------------------------------------------------------------------------------

class Security::SignatureKey
{
public:
    SignatureKey(ReadableView const& cordons) : m_cordons(cordons) {}

    SignatureKey(SignatureKey const&) = delete;
    SignatureKey& operator=(SignatureKey const&) = delete;

    SignatureKey(SignatureKey&& other) noexcept : m_cordons(std::exchange(other.m_cordons, {})) {}

    SignatureKey& operator=(SignatureKey&& other) noexcept {
        if (this != &other) {
            m_cordons = std::exchange(other.m_cordons, {});
        }
        return *this;
    }

    [[nodiscard]] bool operator==(SignatureKey const& other) const noexcept
    {
        return std::ranges::equal(m_cordons, other.m_cordons);
    }

    [[nodiscard]] std::strong_ordering operator<=>(SignatureKey const& other) const noexcept
    {
        return std::lexicographical_compare_three_way(
            m_cordons.begin(), m_cordons.end(), other.m_cordons.begin(), other.m_cordons.end());
    }

    [[nodiscard]] std::uint8_t const* GetData() const { return m_cordons.data(); }
    [[nodiscard]] std::size_t GetSize() const { return m_cordons.size(); }
    [[nodiscard]] bool IsEmpty() const { return m_cordons.empty(); }

    void Erase() { m_cordons = {}; }

private:
    ReadableView m_cordons;
};

//----------------------------------------------------------------------------------------------------------------------

class Security::KeyStore
{
public:
    static constexpr std::size_t PrincipalRandomSize = 32;

    explicit KeyStore(PublicKey&& publicKey);

    KeyStore(KeyStore const& other) = delete;
    KeyStore& operator=(KeyStore const& other) = delete;
    
    KeyStore(KeyStore&& other) noexcept;
    KeyStore& operator=(KeyStore&& other) noexcept;

    ~KeyStore();

    [[nodiscard]] PublicKey const& GetPublicKey() const;
    [[nodiscard]] OptionalPublicKey const& GetPeerPublicKey() const;
    [[nodiscard]] std::size_t GetPublicKeySize() const;

    [[nodiscard]] Salt const& GetSalt() const;
    [[nodiscard]] std::size_t GetSaltSize() const;

    [[nodiscard]] OptionalEncryptionKey const& GetContentKey() const;
    [[nodiscard]] OptionalEncryptionKey const& GetPeerContentKey() const;

    [[nodiscard]] OptionalSignatureKey const& GetSignatureKey() const;
    [[nodiscard]] OptionalSignatureKey const& GetPeerSignatureKey() const;

    [[nodiscard]] std::size_t GetVerificationDataSize() const;

    [[nodiscard]] bool HasGeneratedKeys() const;
    
    void SetPeerPublicKey(PublicKey&& publicKey);
    void PrependSessionSalt(Salt salt);
    void AppendSessionSalt(Salt const& salt);

    [[nodiscard]] OptionalSecureBuffer GenerateSessionKeys(
        ExchangeRole role, CipherSuite const& cipherSuite, SharedSecret const& sharedSecret);

    void Erase();

private:
    struct KeyDerivationFunctionDeleter
    {
        void operator()(EVP_KDF* pKDF) const
        {
            EVP_KDF_free(pKDF);
        }
    };

    using KeyDerivationFunction = std::unique_ptr<EVP_KDF, KeyDerivationFunctionDeleter>;

    [[nodiscard]] std::size_t SetInitiatorKeyCordons(std::size_t contentKeySize, std::size_t signatureKeySize);
    [[nodiscard]] std::size_t SetAcceptorKeyCordons(std::size_t contentKeySize, std::size_t signatureKeySize);
    [[nodiscard]] OptionalSecureBuffer GenerateVerificationData(ReadableView key);

    KeyDerivationFunction m_upKeyDerivationFunction;

    EVP_MD const* m_pVerificationDigest;
    std::size_t m_verificationDataSize;
    
    PublicKey m_publicKey;
    OptionalPublicKey m_optPeerPublicKey;

    Salt m_salt;

    OptionalPrincipalKey m_optPrincipalKey;
    OptionalEncryptionKey m_optContentKey;
    OptionalEncryptionKey m_optPeerContentKey;
    OptionalSignatureKey m_optSignatureKey;
    OptionalSignatureKey m_optPeerSignatureKey;

    bool m_bHasGeneratedKeys;
};

//----------------------------------------------------------------------------------------------------------------------
