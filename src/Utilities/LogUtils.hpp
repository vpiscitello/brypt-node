//----------------------------------------------------------------------------------------------------------------------
// File: LogUtils.hpp
// Description: 
//----------------------------------------------------------------------------------------------------------------------
#pragma once
//----------------------------------------------------------------------------------------------------------------------
#define SPDLOG_DISABLE_DEFAULT_LOGGER
#define SPDLOG_NO_THREAD_ID
//----------------------------------------------------------------------------------------------------------------------
#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>
//----------------------------------------------------------------------------------------------------------------------
#include <sstream>
#include <string>
#include <string_view>
#include <vector>
//----------------------------------------------------------------------------------------------------------------------

//----------------------------------------------------------------------------------------------------------------------
namespace LogUtils {
//----------------------------------------------------------------------------------------------------------------------

void InitializeLoggers(spdlog::level::level_enum verbosity = spdlog::level::debug);

using Logger = std::shared_ptr<spdlog::logger>;

//----------------------------------------------------------------------------------------------------------------------
namespace Name {
//----------------------------------------------------------------------------------------------------------------------

constexpr std::string_view Core = "core";
constexpr std::string_view TcpServer = "tcp-server";
constexpr std::string_view TcpClient = "tcp-client";

//----------------------------------------------------------------------------------------------------------------------
} // Loggers namespace
//----------------------------------------------------------------------------------------------------------------------
namespace Pattern {
//----------------------------------------------------------------------------------------------------------------------

constexpr std::string_view Prefix = "==";
constexpr std::string_view TagOpen = "[";
constexpr std::string_view TagClose = "]";
constexpr std::string_view TagSeperator = " ";
constexpr std::string_view Date = "[%a, %d %b %Y %T]";
constexpr std::string_view Message = "%^[%l] - %v%$";

std::string Generate(std::string_view name, std::vector<std::string> const& tags);

//----------------------------------------------------------------------------------------------------------------------
} // Pattern namespace
//----------------------------------------------------------------------------------------------------------------------
namespace Color {
//----------------------------------------------------------------------------------------------------------------------

constexpr std::string_view Core = "\x1b[1;38;2;0;255;175m";
constexpr std::string_view TCP = "\x1b[1;38;2;0;195;255m";
    
constexpr spdlog::string_view_t Info = "\x1b[38;2;26;204;148m";
constexpr spdlog::string_view_t Warn = "\x1b[38;2;255;214;102m";
constexpr spdlog::string_view_t Error = "\x1b[38;2;255;56;56m";
constexpr spdlog::string_view_t Critical = "\x1b[1;38;2;255;56;56m";
constexpr spdlog::string_view_t Debug = "\x1b[38;2;45;204;255m";
constexpr spdlog::string_view_t Trace = "\x1b[38;2;255;255;255m";

constexpr std::string_view Reset = "\x1b[0m";

std::shared_ptr<spdlog::sinks::stdout_color_sink_mt> CreateTrueColorConsole();

//----------------------------------------------------------------------------------------------------------------------
} // Color namespace
} // FileUtils namespace
//----------------------------------------------------------------------------------------------------------------------

inline void LogUtils::InitializeLoggers(spdlog::level::level_enum verbosity)
{
    auto spCoreLogger = std::make_shared<spdlog::logger>(
        Name::Core.data(), Color::CreateTrueColorConsole()); 
    spCoreLogger->set_pattern(Pattern::Generate(Color::Core, {"core"}));
    spdlog::register_logger(spCoreLogger);

    auto spTcpServerLogger = std::make_shared<spdlog::logger>(
        Name::TcpServer.data(), Color::CreateTrueColorConsole());
    spTcpServerLogger->set_pattern(Pattern::Generate(Color::TCP, {"tcp", "server"}));
    spdlog::register_logger(spTcpServerLogger);

    auto spTcpClientLogger = std::make_shared<spdlog::logger>(
        Name::TcpClient.data(), Color::CreateTrueColorConsole());
    spTcpClientLogger->set_pattern(Pattern::Generate(Color::TCP, {"tcp", "client"}));
    spdlog::register_logger(spTcpClientLogger);

    spdlog::set_level(verbosity);
}

//----------------------------------------------------------------------------------------------------------------------
inline std::string LogUtils::Pattern::Generate(
    std::string_view color, std::vector<std::string> const& tags)
{
    std::ostringstream oss;
    oss << Prefix << TagSeperator << Date << TagSeperator;
    for (auto const& tag : tags) {
        oss << TagOpen << color << tag << Color::Reset << TagClose << TagSeperator;
    }
    oss << Message;
    return oss.str();
}

//----------------------------------------------------------------------------------------------------------------------

inline std::shared_ptr<spdlog::sinks::stdout_color_sink_mt> LogUtils::Color::CreateTrueColorConsole()
{
    auto spColorSink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();

    spColorSink->set_color_mode(spdlog::color_mode::always);
    spColorSink->set_color(spdlog::level::info, Color::Info);
    spColorSink->set_color(spdlog::level::warn, Color::Warn);
    spColorSink->set_color(spdlog::level::err, Color::Error);
    spColorSink->set_color(spdlog::level::critical, Color::Critical);
    spColorSink->set_color(spdlog::level::debug, Color::Debug);
    spColorSink->set_color(spdlog::level::trace, Color::Trace);

    return spColorSink;
}

//----------------------------------------------------------------------------------------------------------------------
