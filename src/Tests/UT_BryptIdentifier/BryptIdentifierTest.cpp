//----------------------------------------------------------------------------------------------------------------------
#include "Components/Identifier/BryptIdentifier.hpp"
#include "Components/Identifier/ReservedIdentifiers.hpp"
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

TEST(BryptIdentifierSuite, GenerateTest)
{
    for (std::uint32_t idx = 0; idx < 10000; ++idx) {
        auto const network = Node::GenerateIdentifier();
        auto const optInternalRepresentation = Node::ToInternalIdentifier(network);
        ASSERT_TRUE(optInternalRepresentation);

        auto const optNetworkRepresentation = Node::ToExternalIdentifier(*optInternalRepresentation);
        ASSERT_TRUE(optNetworkRepresentation);
        
        EXPECT_EQ(network, *optNetworkRepresentation);
        EXPECT_TRUE(Node::IsIdentifierAllowed(*optInternalRepresentation));
    }
}

//----------------------------------------------------------------------------------------------------------------------

TEST(BryptIdentifierSuite, FromInternalIdentifierTest)
{
    for (std::uint32_t idx = 0; idx < 10000; ++idx) {
        auto const network = Node::GenerateIdentifier();
        auto const optInternal = Node::ToInternalIdentifier(network);
        ASSERT_TRUE(optInternal);

        Node::Identifier identifier(*optInternal);
        auto const& checkInternal = static_cast<Node::Internal::Identifier const&>(identifier);
        auto const& checkNetwork = static_cast<Node::External::Identifier const&>(identifier);
        EXPECT_EQ(*optInternal, checkInternal);
        EXPECT_EQ(network, checkNetwork);
        EXPECT_TRUE(identifier.IsValid());
    }
}

//----------------------------------------------------------------------------------------------------------------------

TEST(BryptIdentifierSuite, FromExternalIdentifierTest)
{
    for (std::uint32_t idx = 0; idx < 10000; ++idx) {
        auto const network = Node::GenerateIdentifier();
        auto const optInternal = Node::ToInternalIdentifier(network);
        ASSERT_TRUE(optInternal);

        Node::Identifier identifier(network);
        auto const& checkInternal = static_cast<Node::Internal::Identifier const&>(identifier);
        auto const& checkNetwork = static_cast<Node::External::Identifier const&>(identifier);
        EXPECT_EQ(*optInternal, checkInternal);
        EXPECT_EQ(network, checkNetwork);
        EXPECT_TRUE(identifier.IsValid());
    }
}

//----------------------------------------------------------------------------------------------------------------------