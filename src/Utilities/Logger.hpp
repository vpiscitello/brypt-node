//----------------------------------------------------------------------------------------------------------------------
// File: Logger.hpp
// Description: 
//----------------------------------------------------------------------------------------------------------------------
#pragma once
//----------------------------------------------------------------------------------------------------------------------
#include <spdlog/spdlog.h>
#include <spdlog/pattern_formatter.h>
#include <spdlog/sinks/base_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>
//----------------------------------------------------------------------------------------------------------------------
#include <sstream>
#include <string>
#include <string_view>
#include <vector>
//----------------------------------------------------------------------------------------------------------------------

//----------------------------------------------------------------------------------------------------------------------
namespace Logger {
//----------------------------------------------------------------------------------------------------------------------

constexpr std::string_view Name = "core";
constexpr std::string_view MessagePattern = "%v%$";

void Initialize(spdlog::level::level_enum verbosity, bool useStdOutSink);
void AttachSink(std::shared_ptr<spdlog::sinks::sink> const& spSink);

//----------------------------------------------------------------------------------------------------------------------
} // Logger namespace
//----------------------------------------------------------------------------------------------------------------------

inline void Logger::Initialize(spdlog::level::level_enum verbosity, bool useStdOutSink)
{
    if (auto const spCore = spdlog::get(Name.data()); !spCore) {
        spdlog::register_logger(std::make_shared<spdlog::logger>(Name.data()));
    
        if (useStdOutSink) {
            auto const spCoreSink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
            AttachSink(spCoreSink);
        }

        spdlog::set_level(verbosity);
    }
}

//----------------------------------------------------------------------------------------------------------------------

inline void Logger::AttachSink(std::shared_ptr<spdlog::sinks::sink> const& spSink)
{
    auto spCore = spdlog::get(Name.data());
    assert(spCore);
    spCore->sinks().emplace_back(spSink);
}

//----------------------------------------------------------------------------------------------------------------------
