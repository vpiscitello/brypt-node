//----------------------------------------------------------------------------------------------------------------------
// File: SynchronizerModel.hpp
// Description: 
//----------------------------------------------------------------------------------------------------------------------
#pragma once
//----------------------------------------------------------------------------------------------------------------------
#include "KeyStore.hpp"
#include "SecurityTypes.hpp"
#include "SecurityDefinitions.hpp"
//----------------------------------------------------------------------------------------------------------------------
#include <cstdint>
#include <memory>
//----------------------------------------------------------------------------------------------------------------------

namespace Security { class CipherPackage; }

//----------------------------------------------------------------------------------------------------------------------
namespace Security::Detail {
//----------------------------------------------------------------------------------------------------------------------

class ISynchronizerModel;

//----------------------------------------------------------------------------------------------------------------------
} // Security::Detail namespace
//----------------------------------------------------------------------------------------------------------------------

class Security::Detail::ISynchronizerModel
{
public:
    using SetupResult = std::optional<PublicKey>;
    using ComputeFromPublicKeyResult = std::optional<std::pair<SharedSecret, SupplementalData>>;
    using ComputeFromSupplementalDataResult = std::optional<SharedSecret>;

    virtual ~ISynchronizerModel() = default;
    [[nodiscard]] virtual SetupResult SetupKeyExchange(CipherSuite const& cipherSuite) = 0;
    [[nodiscard]] virtual ComputeFromPublicKeyResult ComputeSharedSecret(PublicKey const& publicKey) = 0;
    [[nodiscard]] virtual ComputeFromSupplementalDataResult ComputeSharedSecret(SupplementalData const& supplementalData) = 0;

    // Supplemental data is the data that is appended to the request buffer after generating the session keys. 
    [[nodiscard]] virtual bool HasSupplementalData() const = 0;
    [[nodiscard]] virtual std::size_t GetSupplementalDataSize() const = 0;
};

//----------------------------------------------------------------------------------------------------------------------
