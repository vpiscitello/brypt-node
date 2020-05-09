//------------------------------------------------------------------------------------------------
// File: NetworkUtils.hpp
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

void CreateFolderIfNoneExist(std::filesystem::path const& path);
bool IsNewlineOrTab(char c);

//------------------------------------------------------------------------------------------------
} // FileUtils namespace
//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
// Function:
// Description:
//------------------------------------------------------------------------------------------------
inline void FileUtils::CreateFolderIfNoneExist(std::filesystem::path const& path)
{
    auto const filename = (path.has_filename()) ? path.filename() : "";
    auto const base = (!filename.empty()) ? path.parent_path() : path;

    // Create configuration directories if they do not yet exist on the system
    if (!std::filesystem::exists(base)) {
        // Create directories in the base path that do not exist
        bool const status = std::filesystem::create_directories(base);
        if (!status) {
            NodeUtils::printo(
                "There was an error creating the folder: " + base.string(),
                NodeUtils::PrintType::Node);
            return;
        }

        // Set the permissions on the brypt conffiguration folder such that only the 
        // user can read, write, and execute
        std::filesystem::permissions(base, std::filesystem::perms::owner_all);
    }
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
