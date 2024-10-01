//----------------------------------------------------------------------------------------------------------------------
#include "Components/Configuration/Options.hpp"
#include "Components/Core/ServiceProvider.hpp"
#include "Components/Message/ApplicationMessage.hpp"
#include "Components/Message/MessageContext.hpp"
#include "Components/Peer/Proxy.hpp"
#include "Components/Security/Algorithms.hpp"
#include "Components/Security/CipherPackage.hpp"
#include "Components/Security/CipherService.hpp"
#include "Components/Security/PackageSynchronizer.hpp"
#include "Components/Security/KeyStore.hpp"
//----------------------------------------------------------------------------------------------------------------------
#include <gtest/gtest.h>
//----------------------------------------------------------------------------------------------------------------------
#include <memory>
#include <random>
//----------------------------------------------------------------------------------------------------------------------

//----------------------------------------------------------------------------------------------------------------------
namespace {
namespace local {
//----------------------------------------------------------------------------------------------------------------------

//----------------------------------------------------------------------------------------------------------------------
} // local namespace
//----------------------------------------------------------------------------------------------------------------------
namespace test {
//----------------------------------------------------------------------------------------------------------------------

std::string const KeyAgreementName = "kem-kyber768";
std::string const CipherName = "aes-256-ctr";
std::string const HashFunctionName = "sha384";

//----------------------------------------------------------------------------------------------------------------------
} // local namespace
} // namespace
//----------------------------------------------------------------------------------------------------------------------

class CipherServiceSuite : public testing::Test
{
protected:
    static void SetUpTestSuite()
    {
        auto const options = Configuration::Options::SupportedAlgorithms{
            {
                {
                    Security::ConfidentialityLevel::High,
                    Configuration::Options::Algorithms{ "high", { test::KeyAgreementName }, { test::CipherName }, { test::HashFunctionName } }
                }
            }
        };

        m_spServiceProvider = std::make_shared<Node::ServiceProvider>();
        m_spCipherService = std::make_shared<Security::CipherService>(options);
        m_spServiceProvider->Register(m_spCipherService);
    }

    static void TearDownTestSuite()
    {
        m_spServiceProvider.reset();
    }

    void SetUp() override
    {
    }

    static inline std::shared_ptr<Security::CipherService> m_spCipherService = {};
    static inline std::shared_ptr<Node::ServiceProvider> m_spServiceProvider = {};
};

//----------------------------------------------------------------------------------------------------------------------

TEST_F(CipherServiceSuite, GetSupportedAlgoritmsTest)
{
    auto const supportedAlgorithms = m_spCipherService->GetSupportedAlgorithms();
    Configuration::Options::SupportedAlgorithms const expectedAlgorithms{
        {
            {
                Security::ConfidentialityLevel::High,
                Configuration::Options::Algorithms{ "high", { test::KeyAgreementName }, { test::CipherName }, { test::HashFunctionName } }
            }
        }
    };

    EXPECT_EQ(supportedAlgorithms, expectedAlgorithms);
}

//----------------------------------------------------------------------------------------------------------------------

TEST_F(CipherServiceSuite, CreateSynchronizerTest)
{
    auto const upInitiator = m_spCipherService->CreateSynchronizer(Security::ExchangeRole::Initiator);
    EXPECT_EQ(upInitiator->GetExchangeRole(), Security::ExchangeRole::Initiator);

    auto const upAcceptor = m_spCipherService->CreateSynchronizer(Security::ExchangeRole::Acceptor);
    EXPECT_EQ(upAcceptor->GetExchangeRole(), Security::ExchangeRole::Acceptor);

    auto const [initiatorStageZeroStatus, initiatorStageZeroBuffer] = upInitiator->Initialize();
    [[maybe_unused]] auto const [acceptorStageZeroStatus, acceptorStageZeroBuffer] = upAcceptor->Initialize();

    auto const [acceptorStageOneStatus, acceptorStageOneBuffer] = upAcceptor->Synchronize(initiatorStageZeroBuffer);
    auto const [initiatorStageOneStatus, initiatorStageOneBuffer] = upInitiator->Synchronize(acceptorStageOneBuffer);
    auto const [acceptorStageTwoStatus, acceptorStageTwoBuffer] = upAcceptor->Synchronize(initiatorStageOneBuffer);
    auto const [initiatorStageTwoStatus, initiatorStageTwoBuffer] = upInitiator->Synchronize(acceptorStageTwoBuffer);

    // The synchronizers produced should be able to perform synchronization and result in a valid cipher package.  
    auto upInitiatorPackage = upInitiator->Finalize();
    EXPECT_TRUE(upInitiatorPackage);

    auto upAcceptorPackage = upAcceptor->Finalize();
    EXPECT_TRUE(upAcceptorPackage);
}

//----------------------------------------------------------------------------------------------------------------------
