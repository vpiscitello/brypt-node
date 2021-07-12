//----------------------------------------------------------------------------------------------------------------------
#include "Components/Configuration/Options.hpp"
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

auto RuntimeOptions = Configuration::Options::Runtime
{ 
    .context = RuntimeContext::Foreground,
    .verbosity = spdlog::level::debug,
    .useInteractiveConsole = false,
    .useBootstraps = false,
    .useFilepathDeduction = false
};

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
    Configuration::Parser parser(local::GetFilepath("good/config.json"), test::RuntimeOptions);
    EXPECT_EQ(parser.FetchOptions(), Configuration::StatusCode::Success);
}

//----------------------------------------------------------------------------------------------------------------------

TEST(ConfigurationParserSuite, ParseMalformedFileTest)
{
    Configuration::Parser parser(local::GetFilepath("malformed/config.json"), test::RuntimeOptions);
    EXPECT_NE(parser.FetchOptions(), Configuration::StatusCode::Success);
}

//----------------------------------------------------------------------------------------------------------------------

TEST(ConfigurationParserSuite, ParseMissingFileTest)
{
    Configuration::Parser parser(local::GetFilepath("missing/config.json"), test::RuntimeOptions);
    EXPECT_EQ(parser.FetchOptions(), Configuration::StatusCode::FileError);
}

//----------------------------------------------------------------------------------------------------------------------

std::filesystem::path local::GetFilepath(std::filesystem::path const& filename)
{
    auto const pwd = std::filesystem::current_path();
    return (pwd.filename() == "UT_Configuration") ?  
        pwd / "files" / filename :  pwd / "Tests/UT_Configuration/files" / filename;
}

//----------------------------------------------------------------------------------------------------------------------
