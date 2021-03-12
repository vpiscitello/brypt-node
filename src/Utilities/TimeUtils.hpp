//----------------------------------------------------------------------------------------------------------------------
#pragma once
//----------------------------------------------------------------------------------------------------------------------
#include <cstring>
#include <chrono>
#include <iostream>
#include <sstream>
#include <string>
#include <string_view>
//----------------------------------------------------------------------------------------------------------------------

//----------------------------------------------------------------------------------------------------------------------
namespace TimeUtils {
//----------------------------------------------------------------------------------------------------------------------

using Timestamp = std::chrono::milliseconds;
using Timepoint = std::chrono::time_point<std::chrono::system_clock, Timestamp>;

Timepoint GetSystemTimepoint();
Timestamp GetSystemTimestamp();
std::string TimepointToString(Timepoint const& time);
Timestamp TimepointToTimestamp(Timepoint const& time);
Timepoint StringToTimepoint(std::string const& timestamp);

//----------------------------------------------------------------------------------------------------------------------
} // TimeUtils namespace
//----------------------------------------------------------------------------------------------------------------------

//----------------------------------------------------------------------------------------------------------------------

inline TimeUtils::Timepoint TimeUtils::GetSystemTimepoint()
{
    return std::chrono::time_point_cast<Timestamp>(std::chrono::system_clock::now());
}

//----------------------------------------------------------------------------------------------------------------------

inline TimeUtils::Timestamp TimeUtils::GetSystemTimestamp()
{
    Timepoint const timepoint = GetSystemTimepoint();
    auto const milliseconds = std::chrono::duration_cast<Timestamp>(timepoint.time_since_epoch());
    return milliseconds;
}

//----------------------------------------------------------------------------------------------------------------------

inline std::string TimeUtils::TimepointToString(Timepoint const& time)
{
    auto const milliseconds = TimepointToTimestamp(time);

    std::stringstream epochStream;
    epochStream.clear();
    epochStream << milliseconds.count();
    return epochStream.str();
}

//----------------------------------------------------------------------------------------------------------------------

inline TimeUtils::Timestamp TimeUtils::TimepointToTimestamp(Timepoint const& timepoint)
{
    return std::chrono::duration_cast<Timestamp>(timepoint.time_since_epoch());
}

//----------------------------------------------------------------------------------------------------------------------
