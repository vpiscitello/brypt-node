//----------------------------------------------------------------------------------------------------------------------
#include <brypt/brypt.h>
#include <brypt/brypt.hpp>
//----------------------------------------------------------------------------------------------------------------------
#include <gtest/gtest.h>
//----------------------------------------------------------------------------------------------------------------------
#include <thread>
#include <filesystem>
//----------------------------------------------------------------------------------------------------------------------

TEST(LibraryStatusSuite, StatusDefaultConstructorTest)
{
    brypt::status status;
    EXPECT_TRUE(status.is_success());
    EXPECT_FALSE(status.is_error());
    EXPECT_EQ(status, brypt::status_code::accepted);
    EXPECT_EQ(status.value(), BRYPT_ACCEPTED);
    EXPECT_STREQ(status.what(), brypt_error_description(BRYPT_ACCEPTED));
}

//----------------------------------------------------------------------------------------------------------------------

TEST(LibraryStatusSuite, StatusCAPIConstructorTest)
{
    brypt::status status(BRYPT_ETIMEOUT);
    EXPECT_FALSE(status.is_success());
    EXPECT_TRUE(status.is_error());
    EXPECT_EQ(status, brypt::status_code::timeout);
    EXPECT_EQ(status.value(), BRYPT_ETIMEOUT);
    EXPECT_STREQ(status.what(), brypt_error_description(BRYPT_ETIMEOUT));
}

//----------------------------------------------------------------------------------------------------------------------

TEST(LibraryStatusSuite, StatusSuccessCodeConstructorTest)
{
    brypt::status status(brypt::status_code::accepted);
    EXPECT_TRUE(status.is_success());
    EXPECT_FALSE(status.is_error());
    EXPECT_EQ(status, brypt::status_code::accepted);
    EXPECT_EQ(status.value(), BRYPT_ACCEPTED);
    EXPECT_STREQ(status.what(), brypt_error_description(BRYPT_ACCEPTED));
}

//----------------------------------------------------------------------------------------------------------------------

TEST(LibraryStatusSuite, StatusErrorCodeConstructorTest)
{
    brypt::status status(brypt::status_code::unspecified);
    EXPECT_FALSE(status.is_success());
    EXPECT_TRUE(status.is_error());
    EXPECT_EQ(status, brypt::status_code::unspecified);
    EXPECT_EQ(status.value(), BRYPT_EUNSPECIFIED);
    EXPECT_STREQ(status.what(), brypt_error_description(BRYPT_EUNSPECIFIED));
}

//----------------------------------------------------------------------------------------------------------------------

TEST(LibraryStatusSuite, StatusComparisonTest)
{
    brypt::status status(BRYPT_EFILENOTFOUND);
    EXPECT_EQ(status, brypt::status(brypt::status_code::file_not_found));
    EXPECT_EQ(status, brypt::status_code::file_not_found);
    EXPECT_NE(status, brypt::status_code::accepted);
}

//----------------------------------------------------------------------------------------------------------------------
