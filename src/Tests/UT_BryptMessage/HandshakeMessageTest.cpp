//------------------------------------------------------------------------------------------------
#include "../../BryptIdentifier/BryptIdentifier.hpp"
#include "../../BryptMessage/HandshakeMessage.hpp"
//------------------------------------------------------------------------------------------------
#include "../../Libraries/googletest/include/gtest/gtest.h"
//------------------------------------------------------------------------------------------------
#include <cstdint>
#include <string>
#include <string_view>
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

BryptIdentifier::CContainer const ClientIdentifier(BryptIdentifier::Generate());
BryptIdentifier::CContainer const ServerIdentifier(BryptIdentifier::Generate());

constexpr std::string_view Data = "Hello World!";

//------------------------------------------------------------------------------------------------
} // local namespace
} // namespace
//------------------------------------------------------------------------------------------------

TEST(CHandshakeMessageSuite, BaseConstructorTest)
{
    auto const optMessage = CHandshakeMessage::Builder()
        .SetSource(test::ClientIdentifier)
        .SetDestination(test::ServerIdentifier)
        .SetData(test::Data)
        .ValidatedBuild();
    ASSERT_TRUE(optMessage);

    EXPECT_EQ(optMessage->GetSourceIdentifier(), test::ClientIdentifier);
    ASSERT_TRUE(optMessage->GetDestinationIdentifier());
    EXPECT_EQ(*optMessage->GetDestinationIdentifier(), test::ServerIdentifier);

    auto const buffer = optMessage->GetData();
    std::string const data(buffer.begin(), buffer.end());
    EXPECT_EQ(data, test::Data);

    auto const pack = optMessage->GetPack();
    EXPECT_EQ(pack.size(), optMessage->GetPackSize());
}

//------------------------------------------------------------------------------------------------

TEST(CHandshakeMessageSuite, PackConstructorTest)
{
    auto const optBaseMessage = CHandshakeMessage::Builder()
        .SetSource(test::ClientIdentifier)
        .SetDestination(test::ServerIdentifier)
        .SetData(test::Data)
        .ValidatedBuild();
    ASSERT_TRUE(optBaseMessage);

    auto const pack = optBaseMessage->GetPack();

    auto const optPackMessage = CHandshakeMessage::Builder()
        .FromEncodedPack(pack)
        .ValidatedBuild();
    ASSERT_TRUE(optPackMessage);

    EXPECT_EQ(optPackMessage->GetSourceIdentifier(), optBaseMessage->GetSourceIdentifier());
    ASSERT_TRUE(optPackMessage->GetDestinationIdentifier());
    EXPECT_EQ(*optPackMessage->GetDestinationIdentifier(), *optBaseMessage->GetDestinationIdentifier());
    EXPECT_EQ(optPackMessage->GetData(), optBaseMessage->GetData());

    auto const buffer = optPackMessage->GetData();
    std::string const data(buffer.begin(), buffer.end());
    EXPECT_EQ(data, test::Data);
}

//------------------------------------------------------------------------------------------------
