//----------------------------------------------------------------------------------------------------------------------
#include <brypt/brypt.h>
#include <brypt/brypt.hpp>
//----------------------------------------------------------------------------------------------------------------------
#include <gtest/gtest.h>
//----------------------------------------------------------------------------------------------------------------------
#include <thread>
#include <filesystem>
//----------------------------------------------------------------------------------------------------------------------

TEST(BryptLibrarySuite, StatusDefaultConstructorTest)
{
    brypt::status status;
    EXPECT_TRUE(status.is_nominal());
    EXPECT_FALSE(status.is_error());
    EXPECT_EQ(status, brypt::success_code::accepted);
    EXPECT_EQ(status.value(), BRYPT_ACCEPTED);
    EXPECT_STREQ(status.what(), brypt_error_description(BRYPT_ACCEPTED));
}

//----------------------------------------------------------------------------------------------------------------------

TEST(BryptLibrarySuite, StatusCAPIConstructorTest)
{
    brypt::status status(BRYPT_EOPERTIMEOUT);
    EXPECT_FALSE(status.is_nominal());
    EXPECT_TRUE(status.is_error());
    EXPECT_EQ(status, brypt::error_code::operation_timeout);
    EXPECT_EQ(status.value(), BRYPT_EOPERTIMEOUT);
    EXPECT_STREQ(status.what(), brypt_error_description(BRYPT_EOPERTIMEOUT));
}

//----------------------------------------------------------------------------------------------------------------------

TEST(BryptLibrarySuite, StatusSuccessCodeConstructorTest)
{
    brypt::status status(brypt::success_code::accepted);
    EXPECT_TRUE(status.is_nominal());
    EXPECT_FALSE(status.is_error());
    EXPECT_EQ(status, brypt::success_code::accepted);
    EXPECT_EQ(status.value(), BRYPT_ACCEPTED);
    EXPECT_STREQ(status.what(), brypt_error_description(BRYPT_ACCEPTED));
}

//----------------------------------------------------------------------------------------------------------------------

TEST(BryptLibrarySuite, StatusErrorCodeConstructorTest)
{
    brypt::status status(brypt::error_code::unspecified);
    EXPECT_FALSE(status.is_nominal());
    EXPECT_TRUE(status.is_error());
    EXPECT_EQ(status, brypt::error_code::unspecified);
    EXPECT_EQ(status.value(), BRYPT_EUNSPECIFIED);
    EXPECT_STREQ(status.what(), brypt_error_description(BRYPT_EUNSPECIFIED));
}

//----------------------------------------------------------------------------------------------------------------------

TEST(BryptLibrarySuite, StatusComparisonTest)
{
    brypt::status status(BRYPT_EFILENOTFOUND);
    EXPECT_EQ(status, brypt::status(brypt::error_code::file_not_found));
    EXPECT_EQ(status, brypt::error_code::file_not_found);
    EXPECT_NE(status, brypt::success_code::accepted);
}

//----------------------------------------------------------------------------------------------------------------------
