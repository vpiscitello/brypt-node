//------------------------------------------------------------------------------------------------
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
//------------------------------------------------------------------------------------------------
#pragma once
//------------------------------------------------------------------------------------------------
#include "SecurityTypes.hpp"
#include "SecureBuffer.hpp"
//------------------------------------------------------------------------------------------------
#include <cstdint>
#include <memory>
#include <optional>
#include <utility>
//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
namespace Security {
//------------------------------------------------------------------------------------------------

using KeyResult = std::pair<std::uint8_t const*, std::uint32_t>;
using OptionalKeyResult = std::optional<KeyResult>;

class CKeyStore;
class CSecureBuffer;

//------------------------------------------------------------------------------------------------
} // Security namespace
//------------------------------------------------------------------------------------------------

class Security::CKeyStore
{
public:
    CKeyStore();

    CKeyStore(CKeyStore&& other) = delete;
    CKeyStore(CKeyStore const& other) = delete;
    CKeyStore& operator=(CKeyStore const& other) = delete;

    void SetPublicKey(Security::Buffer&& buffer);
    void SetPeerPublicKey(Security::Buffer&& buffer);
    void ExpandSessionSeed(Security::Buffer const& buffer);

    bool GenerateSessionKeys(
        Security::Role role,
        Security::Buffer&& buffer,
        std::uint32_t contentKeyBytes,
        std::uint32_t signatureKeyBytes);

    Security::OptionalBuffer GetPeerPublicKey() const;

    Security::OptionalKeyResult GetContentKey() const;
    Security::OptionalKeyResult GetPeerContentKey() const;

    Security::OptionalKeyResult GetSignatureKey() const;
    Security::OptionalKeyResult GetPeerSignatureKey() const;
    
    Security::OptionalBuffer GetVerificationData() const;

    bool HasGeneratedKeys() const;

private:
    using OptionalSecureBuffer = std::optional<Security::CSecureBuffer>;
    
    using KeyCordons = std::pair<std::uint32_t, std::uint32_t>;
    using OptionalKeyCordons = std::optional<KeyCordons>;

    std::uint32_t SetInitiatorKeyCordons(
        std::uint32_t contentKeyBytes, std::uint32_t signatureKeyBytes);
    std::uint32_t SetAcceptorKeyCordons(
        std::uint32_t contentKeyBytes, std::uint32_t signatureKeyBytes);
    
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

//------------------------------------------------------------------------------------------------
