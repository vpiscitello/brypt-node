//----------------------------------------------------------------------------------------------------------------------
// File: BufferPrinter.hpp
//----------------------------------------------------------------------------------------------------------------------
#pragma once
//----------------------------------------------------------------------------------------------------------------------
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <span>
//---------------------------------------------------------------------------------------------------------------------

inline void PrintViewAsHex(std::span<std::uint8_t const> data)
{
    constexpr std::uint32_t rowSize = 32; 

    for (std::size_t idx = 0; idx < data.size(); idx += rowSize) {
        std::cout << std::hex << std::setw(4) << std::setfill('0') << idx << ": ";

        for (std::size_t jdx = 0; jdx < rowSize && (idx + jdx) < data.size(); ++jdx) {
            std::cout << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(data[idx + jdx]) << " ";
        }

        std::cout << std::endl;
    }
}

//---------------------------------------------------------------------------------------------------------------------
