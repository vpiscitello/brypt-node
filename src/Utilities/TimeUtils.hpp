//------------------------------------------------------------------------------------------------
#pragma once
//------------------------------------------------------------------------------------------------
#include <cstring>
#include <chrono>
#include <iostream>
#include <sstream>
#include <string>
#include <string_view>
//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
namespace TimeUtils {
//------------------------------------------------------------------------------------------------

using Timepoint = std::chrono::system_clock::time_point;
using Timestamp = std::chrono::milliseconds;

Timepoint GetSystemTimepoint();
std::string GetSystemTimestamp();
std::string TimepointToString(Timepoint const& time);
Timestamp TimepointToTimestamp(Timepoint const& time);
Timepoint StringToTimepoint(std::string const& timestamp);

//------------------------------------------------------------------------------------------------
} // TimeUtils namespace
//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------

inline TimeUtils::Timepoint TimeUtils::GetSystemTimepoint()
{
    return std::chrono::system_clock::now();
}

//------------------------------------------------------------------------------------------------

inline std::string TimeUtils::GetSystemTimestamp()
{
    Timepoint const current = GetSystemTimepoint();
    auto const milliseconds = std::chrono::duration_cast<Timestamp>(current.time_since_epoch());

    std::stringstream epochStream;
    epochStream.clear();
    epochStream << milliseconds.count();
    return epochStream.str();
}

//------------------------------------------------------------------------------------------------

inline std::string TimeUtils::TimepointToString(Timepoint const& time)
{
    auto const milliseconds = TimepointToTimestamp(time);

    std::stringstream epochStream;
    epochStream.clear();
    epochStream << milliseconds.count();
    return epochStream.str();
}

//------------------------------------------------------------------------------------------------

inline TimeUtils::Timestamp TimeUtils::TimepointToTimestamp(Timepoint const& timepoint)
{
    return std::chrono::duration_cast<Timestamp>(timepoint.time_since_epoch());
}

//------------------------------------------------------------------------------------------------

inline TimeUtils::Timepoint TimeUtils::StringToTimepoint(std::string const& timestamp)
{
    std::int64_t const llMilliseconds = std::stoll(timestamp);
    Timestamp const milliseconds(llMilliseconds);
    return std::chrono::system_clock::time_point(milliseconds);
}

//------------------------------------------------------------------------------------------------
