//----------------------------------------------------------------------------------------------------------------------
#include "BryptMessage/Payload.hpp"
//----------------------------------------------------------------------------------------------------------------------
#include <gtest/gtest.h>
//----------------------------------------------------------------------------------------------------------------------
#include <array>
#include <cstddef>
#include <cstdint>
#include <chrono>
#include <numbers>
#include <string>
#include <string_view>
#include <span>
#include <vector>
//----------------------------------------------------------------------------------------------------------------------

#include <chrono>
#include <iostream>

//----------------------------------------------------------------------------------------------------------------------
namespace {
namespace local {
//----------------------------------------------------------------------------------------------------------------------

//----------------------------------------------------------------------------------------------------------------------
} // local namespace
//----------------------------------------------------------------------------------------------------------------------
namespace test {
//----------------------------------------------------------------------------------------------------------------------

constexpr std::string_view StringPayload = "payload";
auto const BufferPayload = Message::Buffer{ 0x70, 0x61, 0x79, 0x6c, 0x6f, 0x61, 0x64 };
auto const EmptyPayload = Message::Buffer{ 0x00, 0x00, 0x00, 0x00 };

constexpr std::size_t ExpectedPackSize = 11;
constexpr auto ExpectedPackBuffer = std::array<std::uint8_t, ExpectedPackSize>{ 
    0x00, 0x00, 0x00, 0x07, 0x70, 0x61, 0x79, 0x6c, 0x6f, 0x61, 0x64
};

using UnpackableExpectations = std::vector<std::tuple<Message::Buffer, bool>>;

//----------------------------------------------------------------------------------------------------------------------
} // local namespace
} // namespace
//----------------------------------------------------------------------------------------------------------------------

class MessagePayloadSuite : public testing::Test
{
protected:
    void VerifyPayloadStorage()
    {
        EXPECT_TRUE(std::ranges::equal(
            m_payload.GetReadableView(),
            std::span{ reinterpret_cast<std::uint8_t const*>(test::StringPayload.data()), test::StringPayload.size() }));
        EXPECT_EQ(m_payload.GetStringView(), test::StringPayload); 
        EXPECT_FALSE(m_payload.IsEmpty());

        EXPECT_EQ(m_payload.GetPackSize(), test::ExpectedPackSize);

        Message::Buffer injectable;
        m_payload.Inject(injectable);
        EXPECT_TRUE(std::ranges::equal(std::span{ injectable }, test::ExpectedPackBuffer));

        test::UnpackableExpectations const expectations = {
            // Buffer packed correctly.
            { { 0x00, 0x00, 0x00, 0x07, 0x70, 0x61, 0x79, 0x6c, 0x6f, 0x61, 0x64 }, true },
            // Buffer missing data.
            { { 0x00, 0x00, 0x00, 0x07, 0x70, 0x61, 0x79, 0x6c, 0x6f, 0x61 }, false },
            // Buffer missing size field.
            { { 0x70, 0x61, 0x79, 0x6c, 0x6f, 0x61, 0x64 }, false },
            // Buffer with mismatched size field, large.
            { { 0xFF, 0xFF, 0xFF, 0xFF, 0x70, 0x61, 0x79, 0x6c, 0x6f, 0x61, 0x64 }, false },
        };

        for (auto const& [buffer, unpackable] : expectations) {
            std::span bufferview{ buffer };
            auto begin = bufferview.begin();
            EXPECT_EQ(m_payload.Unpack(begin, bufferview.end()), unpackable);
        }
    }

     Message::Payload m_payload;
};

//----------------------------------------------------------------------------------------------------------------------

TEST_F(MessagePayloadSuite, StringStorageTypeTest)
{
    m_payload = { test::StringPayload };
    VerifyPayloadStorage();
}

//----------------------------------------------------------------------------------------------------------------------

TEST_F(MessagePayloadSuite, VectorStorageTypeTest)
{
    m_payload = { test::BufferPayload };
    VerifyPayloadStorage();
}

//----------------------------------------------------------------------------------------------------------------------

TEST_F(MessagePayloadSuite, SharedStringStorageTypeTest)
{
    auto const spSharedPayload = std::make_shared<std::string const>(test::StringPayload);
    m_payload = { spSharedPayload };
    VerifyPayloadStorage();
}

//----------------------------------------------------------------------------------------------------------------------

TEST_F(MessagePayloadSuite, SharedVectorStorageTypeTest)
{
    auto const spSharedPayload = std::make_shared<Message::Buffer const>(test::BufferPayload);
    m_payload = { spSharedPayload };
    VerifyPayloadStorage();
}

//----------------------------------------------------------------------------------------------------------------------

TEST_F(MessagePayloadSuite, NullStorageTypeTest)
{
    m_payload = { nullptr };
    EXPECT_TRUE(m_payload.GetReadableView().empty());
    EXPECT_TRUE(m_payload.GetStringView().empty());
    EXPECT_TRUE(m_payload.IsEmpty());
    EXPECT_EQ(m_payload.GetPackSize(), std::size_t{ 4 });

    Message::Buffer injectable;
    m_payload.Inject(injectable);
    EXPECT_TRUE(std::ranges::equal(injectable, test::EmptyPayload));
}

//----------------------------------------------------------------------------------------------------------------------
