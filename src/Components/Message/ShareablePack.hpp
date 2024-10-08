//----------------------------------------------------------------------------------------------------------------------
// File: ShareablePack.hpp
// Description:
//----------------------------------------------------------------------------------------------------------------------
#pragma once
//----------------------------------------------------------------------------------------------------------------------
#include <memory>
#include <string>
//----------------------------------------------------------------------------------------------------------------------

//----------------------------------------------------------------------------------------------------------------------
namespace Message {
//----------------------------------------------------------------------------------------------------------------------

using ShareablePack = std::shared_ptr<std::string const>;

//----------------------------------------------------------------------------------------------------------------------
} // Message namespace
//----------------------------------------------------------------------------------------------------------------------
