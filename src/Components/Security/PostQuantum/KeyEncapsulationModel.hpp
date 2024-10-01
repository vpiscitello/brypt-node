//----------------------------------------------------------------------------------------------------------------------
// File: KeyEncapsulationModel.hpp
// Description: 
//----------------------------------------------------------------------------------------------------------------------
#pragma once
//----------------------------------------------------------------------------------------------------------------------
#include "Components/Security/SecurityTypes.hpp"
#include "Components/Security/SecurityDefinitions.hpp"
#include "Components/Security/SynchronizerContext.hpp"
#include "Components/Security/SynchronizerModel.hpp"
//----------------------------------------------------------------------------------------------------------------------
#ifndef LIBOQS_CPP_VERSION
#define LIBOQS_CPP_VERSION "0.8.0"
#endif
#include <iostream>
#include <oqscpp/oqs_cpp.hpp>
//----------------------------------------------------------------------------------------------------------------------
#include <cstdint>
#include <functional>
#include <memory>
#include <shared_mutex>
#include <string_view>
#include <vector>
//----------------------------------------------------------------------------------------------------------------------

namespace Security { class CipherPackage; }

//----------------------------------------------------------------------------------------------------------------------
namespace Security::Quantum {
//----------------------------------------------------------------------------------------------------------------------

class KeyEncapsulationModel;

//----------------------------------------------------------------------------------------------------------------------
} // Security::Quantum namespace
//----------------------------------------------------------------------------------------------------------------------

class Security::Quantum::KeyEncapsulationModel : public Detail::ISynchronizerModel
{
public:
    KeyEncapsulationModel();

    [[nodiscard]] static bool IsKeyAgreementSupported(std::string const& keyAgreement);

    [[nodiscard]] virtual SetupResult SetupKeyExchange(CipherSuite const& cipherSuite) override;
    [[nodiscard]] virtual ComputeFromPublicKeyResult ComputeSharedSecret(PublicKey const& publicKey) override;
    [[nodiscard]] virtual ComputeFromSupplementalDataResult ComputeSharedSecret(SupplementalData const& supplementalData) override;

    // Supplemental data is the data that is appended to the request buffer after generating the session keys. 
    [[nodiscard]] virtual bool HasSupplementalData() const override;
    [[nodiscard]] virtual std::size_t GetSupplementalDataSize() const override;

private:
    class KeyEncapsulator
    {
    public:
        explicit KeyEncapsulator(oqs::KeyEncapsulation&& kem);
    
        KeyEncapsulator(KeyEncapsulator const&) = delete;
        KeyEncapsulator(KeyEncapsulator&&) = delete;
        void operator=(KeyEncapsulator const&) = delete;
        void operator=(KeyEncapsulator&&) = delete;

        [[nodiscard]] std::size_t GetPublicKeySize() const;
        [[nodiscard]] std::size_t GetEncapsulatedSize() const;
        [[nodiscard]] ComputeFromPublicKeyResult GenerateEncapsulatedSecret(PublicKey const& publicKey) const;
        [[nodiscard]] ComputeFromSupplementalDataResult DecapsulateSecret(SupplementalData const& supplementalData) const;

    private:
        // TODO: I don't think we need a mutex, but need to be sure. 
        //mutable std::shared_mutex m_mutex;
        oqs::KeyEncapsulation m_kem;
    };

    std::unique_ptr<KeyEncapsulator> m_upSessionEncapsulator;
};

//----------------------------------------------------------------------------------------------------------------------
