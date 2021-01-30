//------------------------------------------------------------------------------------------------
// File: FileUtils.hpp
// Description: 
//------------------------------------------------------------------------------------------------
#pragma once
//------------------------------------------------------------------------------------------------
#include "NodeUtils.hpp"
//------------------------------------------------------------------------------------------------
#include <filesystem>
//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
namespace FileUtils {
//------------------------------------------------------------------------------------------------

bool CreateFolderIfNoneExist(std::filesystem::path const& path);
bool IsNewlineOrTab(char c);

//------------------------------------------------------------------------------------------------
} // FileUtils namespace
//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
// Function:
// Description:
//------------------------------------------------------------------------------------------------
inline bool FileUtils::CreateFolderIfNoneExist(std::filesystem::path const& path)
{
    auto const filename = (path.has_filename()) ? path.filename() : "";
    auto const base = (!filename.empty()) ? path.parent_path() : path;

    // If the directories exist, there is nothing to do.
    if (std::filesystem::exists(base)) { return true; }
    
    std::error_code error;
    // Create directories in the base path that do not exist
    bool const success = std::filesystem::create_directories(base, error);
    if (success) {
        // Set the permissions on the brypt conffiguration folder such that only the 
        // user can read, write, and execute
        std::filesystem::permissions(base, std::filesystem::perms::owner_all);
    }
    
    return success;
}

//-----------------------------------------------------------------------------------------------

inline bool FileUtils::IsNewlineOrTab(char c)
{
    switch (c) {
        case '\n':
        case '\t':
            return true;
        default:
            return false;
    }
}

//-----------------------------------------------------------------------------------------------
