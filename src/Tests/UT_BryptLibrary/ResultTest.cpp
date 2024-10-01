//----------------------------------------------------------------------------------------------------------------------
#include <brypt/brypt.h>
#include <brypt/brypt.hpp>
//----------------------------------------------------------------------------------------------------------------------
#include <gtest/gtest.h>
//----------------------------------------------------------------------------------------------------------------------
#include <thread>
#include <filesystem>
//----------------------------------------------------------------------------------------------------------------------

TEST(LibraryResultSuite, ResultDefaultConstructorTest)
{
    brypt::result result;
    EXPECT_TRUE(result.is_success());
    EXPECT_FALSE(result.is_error());
    EXPECT_EQ(result, brypt::result_code::accepted);
    EXPECT_EQ(result.value(), BRYPT_ACCEPTED);
    EXPECT_STREQ(result.what(), brypt_error_description(BRYPT_ACCEPTED));
}

//----------------------------------------------------------------------------------------------------------------------

TEST(LibraryResultSuite, ResultCAPIConstructorTest)
{
    brypt::result result{ BRYPT_ETIMEOUT };
    EXPECT_FALSE(result.is_success());
    EXPECT_TRUE(result.is_error());
    EXPECT_EQ(result, brypt::result_code::timeout);
    EXPECT_EQ(result.value(), BRYPT_ETIMEOUT);
    EXPECT_STREQ(result.what(), brypt_error_description(BRYPT_ETIMEOUT));
}

//----------------------------------------------------------------------------------------------------------------------

TEST(LibraryResultSuite, ResultSuccessCodeConstructorTest)
{
    brypt::result result{ brypt::result_code::accepted };
    EXPECT_TRUE(result.is_success());
    EXPECT_FALSE(result.is_error());
    EXPECT_EQ(result, brypt::result_code::accepted);
    EXPECT_EQ(result.value(), BRYPT_ACCEPTED);
    EXPECT_STREQ(result.what(), brypt_error_description(BRYPT_ACCEPTED));
}

//----------------------------------------------------------------------------------------------------------------------

TEST(LibraryResultSuite, ResultErrorCodeConstructorTest)
{
    brypt::result result{ brypt::result_code::unspecified };
    EXPECT_FALSE(result.is_success());
    EXPECT_TRUE(result.is_error());
    EXPECT_EQ(result, brypt::result_code::unspecified);
    EXPECT_EQ(result.value(), BRYPT_EUNSPECIFIED);
    EXPECT_STREQ(result.what(), brypt_error_description(BRYPT_EUNSPECIFIED));
}

//----------------------------------------------------------------------------------------------------------------------

TEST(LibraryResultSuite, ResultComparisonTest)
{
    brypt::result result{ BRYPT_EFILENOTFOUND };
    EXPECT_EQ(result, brypt::result(brypt::result_code::file_not_found));
    EXPECT_EQ(result, brypt::result_code::file_not_found);
    EXPECT_NE(result, brypt::result_code::accepted);
}

//----------------------------------------------------------------------------------------------------------------------
