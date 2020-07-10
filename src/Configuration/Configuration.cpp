//------------------------------------------------------------------------------------------------
// File: Configuration.cpp 
// Description:
//-----------------------------------------------------------------------------------------------
#include "Configuration.hpp"
//-----------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
namespace {
namespace defaults {
//------------------------------------------------------------------------------------------------

std::filesystem::path const FallbackConfigurationFolder = "/etc/";

//------------------------------------------------------------------------------------------------
} // default namespace
} // namespace
//------------------------------------------------------------------------------------------------

std::filesystem::path Configuration::GetDefaultBryptFolder()
{
    std::string filepath = defaults::FallbackConfigurationFolder; // Set the filepath root to /etc/ by default

    // Attempt to get the $XDG_CONFIG_HOME enviroment variable to use as the configuration directory
    // If $XDG_CONFIG_HOME does not exist try to get the user home directory 
    auto const systemConfigHomePath = std::getenv("XDG_CONFIG_HOME");
    if (systemConfigHomePath) { 
        std::size_t const length = strlen(systemConfigHomePath);
        filepath = std::string(systemConfigHomePath, length);
    } else {
        // Attempt to get the $HOME enviroment variable to use the the configuration directory
        // If $HOME does not exist continue with the fall configuration path
        auto const userHomePath = std::getenv("HOME");
        if (userHomePath) {
            std::size_t const length = strlen(userHomePath);
            filepath = std::string(userHomePath, length) + "/.config";
        } 
    }

    // Concat /brypt/ to the condiguration folder path
    filepath += DefaultBryptFolder;

    return filepath;
}

//-----------------------------------------------------------------------------------------------

std::filesystem::path Configuration::GetDefaultConfigurationFilepath()
{
    return GetDefaultBryptFolder() / DefaultConfigurationFilename; // ../config.json
}

//-----------------------------------------------------------------------------------------------

std::filesystem::path Configuration::GetDefaultPeersFilepath()
{
    return GetDefaultBryptFolder() / DefaultKnownPeersFilename; // ../peers.json
}

//-----------------------------------------------------------------------------------------------
