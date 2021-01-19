//------------------------------------------------------------------------------------------------
#include "BryptIdentifier/BryptIdentifier.hpp"
#include "BryptIdentifier/ReservedIdentifiers.hpp"
//------------------------------------------------------------------------------------------------
#include <gtest/gtest.h>
//------------------------------------------------------------------------------------------------
#include <iostream>
#include <cstdint>
#include <string>
//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
namespace {
namespace local {
//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
} // local namespace
//------------------------------------------------------------------------------------------------
namespace test {
//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
} // local namespace
} // namespace
//------------------------------------------------------------------------------------------------

TEST(BryptIdentifierSuite, BryptIdentifierGenerateTest)
{
    for (std::uint32_t idx = 0; idx < 10000; ++idx) {
        auto const bryptNetworkID = BryptIdentifier::Generate();
        auto const optInternalRepresentation = 
            BryptIdentifier::ConvertToInternalRepresentation(bryptNetworkID);
        ASSERT_TRUE(optInternalRepresentation);

        auto const optNetworkRepresentation = 
            BryptIdentifier::ConvertToNetworkRepresentation(*optInternalRepresentation);
        ASSERT_TRUE(optNetworkRepresentation);
        
        EXPECT_EQ(bryptNetworkID, *optNetworkRepresentation);
        EXPECT_TRUE(ReservedIdentifiers::IsIdentifierAllowed(*optInternalRepresentation));
    }
}

//------------------------------------------------------------------------------------------------

TEST(BryptIdentifierSuite, ContainerFromInternalConstructorTest)
{
    for (std::uint32_t idx = 0; idx < 10000; ++idx) {
        auto const network = BryptIdentifier::Generate();
        auto const optInternal = BryptIdentifier::ConvertToInternalRepresentation(network);
        ASSERT_TRUE(optInternal);

        BryptIdentifier::CContainer identifier(*optInternal);
        auto const checkInternal = identifier.GetInternalRepresentation();
        auto const checkNetwork = identifier.GetNetworkRepresentation();
        EXPECT_EQ(*optInternal, checkInternal);
        EXPECT_EQ(network, checkNetwork);
        EXPECT_TRUE(identifier.IsValid());
    }
}

//------------------------------------------------------------------------------------------------

TEST(BryptIdentifierSuite, ContainerFromNetworkConstructorTest)
{
    for (std::uint32_t idx = 0; idx < 10000; ++idx) {
        auto const network = BryptIdentifier::Generate();
        auto const optInternal = BryptIdentifier::ConvertToInternalRepresentation(network);
        ASSERT_TRUE(optInternal);

        BryptIdentifier::CContainer identifier(network);
        auto const checkInternal = identifier.GetInternalRepresentation();
        auto const checkNetwork = identifier.GetNetworkRepresentation();
        EXPECT_EQ(*optInternal, checkInternal);
        EXPECT_EQ(network, checkNetwork);
        EXPECT_TRUE(identifier.IsValid());
    }
}

//------------------------------------------------------------------------------------------------