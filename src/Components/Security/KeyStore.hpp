//----------------------------------------------------------------------------------------------------------------------
// File: KeyStore.hpp
// Description: The KeyStore is a container for the various public and shared keys 
// required by a security strategy. The KeyStore will take ownership of any keys provided to it
// in order to manage their lifetime. It is expected that the KeyStore is provided a shared 
// secret produced through key exchange or key encapsulatio and will derive content and signature
// secrets from the data. The derived keys will be partitions of the principal derived key, 
// Derived key interfaces (e.g. GetContentKey) will provide the start of the key cordon and 
// the number of bytes in the cordon range. Currently, the key key derivation method is fixed 
// for all security strategies and uses SHAKE-256.  At destruction time shared secrets will be 
// cleared and zeroed in memory. 
//----------------------------------------------------------------------------------------------------------------------
#pragma once
//----------------------------------------------------------------------------------------------------------------------
#include "SecurityTypes.hpp"
#include "SecureBuffer.hpp"
//----------------------------------------------------------------------------------------------------------------------
#include <cstdint>
#include <memory>
#include <optional>
#include <utility>
//----------------------------------------------------------------------------------------------------------------------

//----------------------------------------------------------------------------------------------------------------------
namespace Security {
//----------------------------------------------------------------------------------------------------------------------

using OptionalKeyCordons = std::optional<Security::ReadableView>;

class KeyStore;
class SecureBuffer;

//----------------------------------------------------------------------------------------------------------------------
} // Security namespace
//----------------------------------------------------------------------------------------------------------------------

class Security::KeyStore
{
public:
    constexpr static std::size_t VerificationSize = 32;

    KeyStore();

    KeyStore(KeyStore&& other) = delete;
    KeyStore(KeyStore const& other) = delete;
    KeyStore& operator=(KeyStore const& other) = delete;

    void SetPublicKey(Security::Buffer&& buffer);
    void SetPeerPublicKey(Security::Buffer&& buffer);
    void ExpandSessionSeed(Security::Buffer const& buffer);

    [[nodiscard]] bool GenerateSessionKeys(
        Security::Role role,
        Security::Buffer&& buffer,
        std::size_t contentKeyBytes,
        std::size_t signatureKeyBytes);

    [[nodiscard]] Security::OptionalBuffer GetPeerPublicKey() const;

    [[nodiscard]] Security::OptionalKeyCordons GetContentKey() const;
    [[nodiscard]] Security::OptionalKeyCordons GetPeerContentKey() const;

    [[nodiscard]] Security::OptionalKeyCordons GetSignatureKey() const;
    [[nodiscard]] Security::OptionalKeyCordons GetPeerSignatureKey() const;
    
    [[nodiscard]] Security::OptionalBuffer GetVerificationData() const;

    [[nodiscard]] bool HasGeneratedKeys() const;
    
    void ResetState();

private:
    using OptionalSecureBuffer = std::optional<Security::SecureBuffer>;

    [[nodiscard]] std::size_t SetInitiatorKeyCordons(
        std::size_t contentKeyBytes, std::size_t signatureKeyBytes);
    [[nodiscard]] std::size_t SetAcceptorKeyCordons(
        std::size_t contentKeyBytes, std::size_t signatureKeyBytes);
    
    Security::OptionalBuffer m_optPeerPublicKey;

    Security::Buffer m_seed;
    OptionalSecureBuffer m_optPrinicpalKey;
    OptionalBuffer m_optVerificationData;

    OptionalKeyCordons m_optContentKeyCordons;
    OptionalKeyCordons m_optPeerContentKeyCordons;
    OptionalKeyCordons m_optSignatureKeyCordons;
    OptionalKeyCordons m_optPeerSignatureKeyCordons;
    bool m_bHasGeneratedKeys;
};

//----------------------------------------------------------------------------------------------------------------------
