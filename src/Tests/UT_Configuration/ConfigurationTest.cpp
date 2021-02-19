//------------------------------------------------------------------------------------------------
#include "Components/Configuration/Configuration.hpp"
#include "Components/Configuration/ConfigurationManager.hpp"
#include "Utilities/NodeUtils.hpp"
//------------------------------------------------------------------------------------------------
#include <gtest/gtest.h>
//------------------------------------------------------------------------------------------------
#include <cstdint>
#include <chrono>
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

//------------------------------------------------------------------------------------------------
} // local namespace
} // namespace
//------------------------------------------------------------------------------------------------

TEST(ConfigurationManagerSuite, GenerateConfigurationFilepathTest)
{
    auto const filepath = Configuration::GetDefaultConfigurationFilepath();
    EXPECT_TRUE(filepath.has_parent_path());
    EXPECT_TRUE(filepath.is_absolute());
    auto const found = filepath.string().find(Configuration::DefaultBryptFolder);
    EXPECT_NE(found, std::string::npos);
    EXPECT_EQ(filepath.filename(), Configuration::DefaultConfigurationFilename);
}

//------------------------------------------------------------------------------------------------

TEST(ConfigurationManagerSuite, ParseGoodFileTest)
{
    std::filesystem::path const filepath = "files/good/config.json";
    Configuration::Manager manager(filepath.c_str(), false);
    auto const status = manager.FetchSettings();
    EXPECT_EQ(status, Configuration::StatusCode::Success);
}

//------------------------------------------------------------------------------------------------

TEST(ConfigurationManagerSuite, ParseMalformedFileTest)
{
    std::filesystem::path const filepath = "files/malformed/config.json";
    Configuration::Manager manager(filepath.c_str(), false);
    auto const status = manager.FetchSettings();
    EXPECT_NE(status, Configuration::StatusCode::Success);
}

//------------------------------------------------------------------------------------------------


TEST(ConfigurationManagerSuite, ParseMissingFileTest)
{
    std::filesystem::path const filepath = "files/missing/config.json";
    Configuration::Manager manager(filepath.c_str(), false);
    auto const status = manager.FetchSettings();
    EXPECT_EQ(status, Configuration::StatusCode::FileError);
}

//------------------------------------------------------------------------------------------------