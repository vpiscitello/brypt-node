//----------------------------------------------------------------------------------------------------------------------
#pragma once
//----------------------------------------------------------------------------------------------------------------------
#include <brypt/options.hpp>
#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>
//----------------------------------------------------------------------------------------------------------------------
#include <cstdint>
//----------------------------------------------------------------------------------------------------------------------

//----------------------------------------------------------------------------------------------------------------------
namespace brypt::examples {
//----------------------------------------------------------------------------------------------------------------------

[[nodiscard]] std::shared_ptr<spdlog::logger> generate_logger(std::string_view executable);
[[nodiscard]] spdlog::level::level_enum translate(brypt::log_level level);

//----------------------------------------------------------------------------------------------------------------------
} // brypt::helpers namespace
//----------------------------------------------------------------------------------------------------------------------

inline std::shared_ptr<spdlog::logger> brypt::examples::generate_logger(std::string_view executable)
{
    constexpr std::string_view Prefix = "==";
    constexpr std::string_view TagOpen = "[";
    constexpr std::string_view TagClose = "]";
    constexpr std::string_view TagSeperator = " ";
    constexpr std::string_view Date = "[%a, %d %b %Y %T]";
    constexpr std::string_view Message = "%^[%l] - %v%$";

    std::ostringstream oss;
    oss << Prefix << TagSeperator << Date << TagSeperator;
    for (auto const& tag : { "examples", executable.data() }) {
        oss << TagOpen << tag << TagClose << TagSeperator;
    }
    oss << Message;

    auto const logger = std::make_shared<spdlog::logger>("examples");
    auto const sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();

    sink->set_pattern(oss.str().c_str());
    logger->sinks().emplace_back(sink);

    spdlog::register_logger(logger);
    spdlog::set_level(spdlog::level::trace);

    return logger;
}

//----------------------------------------------------------------------------------------------------------------------

inline spdlog::level::level_enum brypt::examples::translate(brypt::log_level level)
{
    switch (level) {
        case brypt::log_level::off: return spdlog::level::off;
        case brypt::log_level::trace: return spdlog::level::trace;
        case brypt::log_level::debug: return spdlog::level::debug;
        case brypt::log_level::info: return spdlog::level::info;
        case brypt::log_level::warn: return spdlog::level::warn;
        case brypt::log_level::err: return spdlog::level::err;
        case brypt::log_level::critical: return spdlog::level::critical;
        default: break;
    }

    return spdlog::level::level_enum::off;
}

//----------------------------------------------------------------------------------------------------------------------
