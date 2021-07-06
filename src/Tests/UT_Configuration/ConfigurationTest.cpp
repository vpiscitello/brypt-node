//----------------------------------------------------------------------------------------------------------------------
#include "Components/Configuration/Configuration.hpp"
#include "Components/Configuration/Parser.hpp"
#include "Utilities/NodeUtils.hpp"
//----------------------------------------------------------------------------------------------------------------------
#include <gtest/gtest.h>
//----------------------------------------------------------------------------------------------------------------------
#include <cstdint>
#include <chrono>
#include <string>
#include <string_view>
//----------------------------------------------------------------------------------------------------------------------

//----------------------------------------------------------------------------------------------------------------------
namespace {
namespace local {
//----------------------------------------------------------------------------------------------------------------------

std::filesystem::path GetFilepath(std::filesystem::path const& filename);

//----------------------------------------------------------------------------------------------------------------------
} // local namespace
//----------------------------------------------------------------------------------------------------------------------
namespace test {
//----------------------------------------------------------------------------------------------------------------------

//----------------------------------------------------------------------------------------------------------------------
} // local namespace
} // namespace
//----------------------------------------------------------------------------------------------------------------------

TEST(ConfigurationParserSuite, GenerateConfigurationFilepathTest)
{
    auto const filepath = Configuration::GetDefaultConfigurationFilepath();
    EXPECT_TRUE(filepath.has_parent_path());
    EXPECT_TRUE(filepath.is_absolute());
    auto const found = filepath.string().find(Configuration::DefaultBryptFolder);
    EXPECT_NE(found, std::string::npos);
    EXPECT_EQ(filepath.filename(), Configuration::DefaultConfigurationFilename);
}

//----------------------------------------------------------------------------------------------------------------------

TEST(ConfigurationParserSuite, ParseGoodFileTest)
{
    Configuration::Parser parser(local::GetFilepath("good/config.json"), false);
    EXPECT_EQ(parser.FetchSettings(), Configuration::StatusCode::Success);
}

//----------------------------------------------------------------------------------------------------------------------

TEST(ConfigurationParserSuite, ParseMalformedFileTest)
{
    Configuration::Parser parser(local::GetFilepath("malformed/config.json"), false);
    EXPECT_NE(parser.FetchSettings(), Configuration::StatusCode::Success);
}

//----------------------------------------------------------------------------------------------------------------------


TEST(ConfigurationParserSuite, ParseMissingFileTest)
{
    Configuration::Parser parser(local::GetFilepath("missing/config.json"), false);
    EXPECT_EQ(parser.FetchSettings(), Configuration::StatusCode::FileError);
}

//----------------------------------------------------------------------------------------------------------------------

std::filesystem::path local::GetFilepath(std::filesystem::path const& filename)
{
    auto const pwd = std::filesystem::current_path();
    return (pwd.filename() == "UT_Configuration") ?  
        pwd / "files" / filename :  pwd / "Tests/UT_Configuration/files" / filename;
}

//----------------------------------------------------------------------------------------------------------------------
