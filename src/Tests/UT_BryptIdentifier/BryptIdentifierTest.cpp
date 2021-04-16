//----------------------------------------------------------------------------------------------------------------------
#include "BryptIdentifier/BryptIdentifier.hpp"
#include "BryptIdentifier/ReservedIdentifiers.hpp"
//----------------------------------------------------------------------------------------------------------------------
#include <gtest/gtest.h>
//----------------------------------------------------------------------------------------------------------------------
#include <iostream>
#include <cstdint>
#include <string>
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

//----------------------------------------------------------------------------------------------------------------------
} // local namespace
} // namespace
//----------------------------------------------------------------------------------------------------------------------

TEST(BryptIdentifierSuite, BryptIdentifierGenerateTest)
{
    for (std::uint32_t idx = 0; idx < 10000; ++idx) {
        auto const network = Node::GenerateIdentifier();
        auto const optInternalRepresentation = Node::ConvertToInternalRepresentation(network);
        ASSERT_TRUE(optInternalRepresentation);

        auto const optNetworkRepresentation = Node::ConvertToNetworkRepresentation(*optInternalRepresentation);
        ASSERT_TRUE(optNetworkRepresentation);
        
        EXPECT_EQ(network, *optNetworkRepresentation);
        EXPECT_TRUE(Node::IsIdentifierAllowed(*optInternalRepresentation));
    }
}

//----------------------------------------------------------------------------------------------------------------------

TEST(BryptIdentifierSuite, ContainerFromInternalConstructorTest)
{
    for (std::uint32_t idx = 0; idx < 10000; ++idx) {
        auto const network = Node::GenerateIdentifier();
        auto const optInternal = Node::ConvertToInternalRepresentation(network);
        ASSERT_TRUE(optInternal);

        Node::Identifier identifier(*optInternal);
        auto const checkInternal = identifier.GetInternalValue();
        auto const checkNetwork = identifier.GetNetworkString();
        EXPECT_EQ(*optInternal, checkInternal);
        EXPECT_EQ(network, checkNetwork);
        EXPECT_TRUE(identifier.IsValid());
    }
}

//----------------------------------------------------------------------------------------------------------------------

TEST(BryptIdentifierSuite, ContainerFromNetworkConstructorTest)
{
    for (std::uint32_t idx = 0; idx < 10000; ++idx) {
        auto const network = Node::GenerateIdentifier();
        auto const optInternal = Node::ConvertToInternalRepresentation(network);
        ASSERT_TRUE(optInternal);

        Node::Identifier identifier(network);
        auto const checkInternal = identifier.GetInternalValue();
        auto const checkNetwork = identifier.GetNetworkString();
        EXPECT_EQ(*optInternal, checkInternal);
        EXPECT_EQ(network, checkNetwork);
        EXPECT_TRUE(identifier.IsValid());
    }
}

//----------------------------------------------------------------------------------------------------------------------