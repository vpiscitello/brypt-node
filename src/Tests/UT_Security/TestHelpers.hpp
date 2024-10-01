//----------------------------------------------------------------------------------------------------------------------
#include "Components/Security/CipherPackage.hpp"
#include "Components/Security/SecureBuffer.hpp"
#include "Components/Security/SecurityTypes.hpp"
//----------------------------------------------------------------------------------------------------------------------
#include <cassert>
#include <cstdint>
#include <random>
//----------------------------------------------------------------------------------------------------------------------

//----------------------------------------------------------------------------------------------------------------------
namespace Security::Test {
//----------------------------------------------------------------------------------------------------------------------

constexpr ConfidentialityLevel Level = ConfidentialityLevel::High;
constexpr std::string_view KeyAgreementName = "basic-agreement";
constexpr std::string_view CipherName = "aes-256-ctr";
constexpr std::string_view HashFunctionName = "sha384";

[[nodiscard]] Buffer GenerateGarbageData(std::size_t size);
[[nodiscard]] std::unique_ptr<CipherPackage> GenerateCipherPackage();
[[nodiscard]] std::pair<std::unique_ptr<CipherPackage>, std::unique_ptr<CipherPackage>> GenerateCipherPackages();

//----------------------------------------------------------------------------------------------------------------------
} // Route::Test namespace
//----------------------------------------------------------------------------------------------------------------------

inline Security::Buffer Security::Test::GenerateGarbageData(std::size_t size)
{
    std::random_device device;
    std::mt19937 engine(device());
    std::uniform_int_distribution<std::int32_t> distribution(std::numeric_limits<std::uint8_t>::min(), std::numeric_limits<std::uint8_t>::max());

    Security::Buffer data;
    data.reserve(size);

    for (std::size_t idx = 0; idx < size; ++idx) {
        data.emplace_back(static_cast<std::uint8_t>(distribution(engine)));
    }

    return data;
}

//----------------------------------------------------------------------------------------------------------------------

inline std::unique_ptr<Security::CipherPackage> Security::Test::GenerateCipherPackage()
{
    // Create the key store that will be used to create the cipher package 
    Security::KeyStore store{ Security::PublicKey{ Security::Test::GenerateGarbageData(256) } };
    store.SetPeerPublicKey(Security::PublicKey{ Security::Test::GenerateGarbageData(256) });
    store.AppendSessionSalt(Security::Salt{ Security::Test::GenerateGarbageData(256) });

    Security::CipherSuite cipherSuite{ Level, KeyAgreementName, CipherName, HashFunctionName };
    [[maybe_unused]] auto const optVerificationData = store.GenerateSessionKeys(
        Security::ExchangeRole::Acceptor, cipherSuite, Security::Test::GenerateGarbageData(256));
    assert(optVerificationData);

    return std::make_unique<Security::CipherPackage>(cipherSuite, std::move(store));
}

//----------------------------------------------------------------------------------------------------------------------

inline std::pair<std::unique_ptr<Security::CipherPackage>, std::unique_ptr<Security::CipherPackage>> Security::Test::GenerateCipherPackages()
{
    auto const initiatorPublicKey = Security::PublicKey{ Security::Test::GenerateGarbageData(256) };
    auto const acceptorPublicKey = Security::PublicKey{ Security::Test::GenerateGarbageData(256) };
    auto const sharedSecret = Security::SharedSecret{ Security::Test::GenerateGarbageData(256) };

    // Create the key store that will be used to create the cipher package 
    Security::KeyStore initiatorStore{ Security::PublicKey{ initiatorPublicKey } };
    initiatorStore.SetPeerPublicKey(Security::PublicKey{ acceptorPublicKey });

    // Create the key store that will be used to create the cipher package 
    Security::KeyStore acceptorStore{ Security::PublicKey{ acceptorPublicKey } };
    acceptorStore.SetPeerPublicKey(Security::PublicKey{ initiatorPublicKey });

    auto const initialInitiatorSalt = initiatorStore.GetSalt();
    initiatorStore.PrependSessionSalt(acceptorStore.GetSalt());
    acceptorStore.AppendSessionSalt(initialInitiatorSalt);

    Security::CipherSuite cipherSuite{ Level, KeyAgreementName, CipherName, HashFunctionName };

    [[maybe_unused]] auto const optInitiatorVerificationData = initiatorStore.GenerateSessionKeys(
        Security::ExchangeRole::Initiator, cipherSuite, sharedSecret);
    assert(optInitiatorVerificationData);

    [[maybe_unused]] auto const optAcceptorVerificationData = acceptorStore.GenerateSessionKeys(
        Security::ExchangeRole::Acceptor, cipherSuite, sharedSecret);
    assert(optAcceptorVerificationData);
    assert(optInitiatorVerificationData == optAcceptorVerificationData);

    return {
        std::make_unique<Security::CipherPackage>(cipherSuite, std::move(initiatorStore)),
        std::make_unique<Security::CipherPackage>(cipherSuite, std::move(acceptorStore))
    };
}

//----------------------------------------------------------------------------------------------------------------------
