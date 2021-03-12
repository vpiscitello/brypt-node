//----------------------------------------------------------------------------------------------------------------------
#pragma once
//----------------------------------------------------------------------------------------------------------------------
#include <cstdint>
#include <string>
#include <string_view>
#include <utility>
#include <vector>
//----------------------------------------------------------------------------------------------------------------------
#include <boost/program_options.hpp>
#include <spdlog/common.h>
//----------------------------------------------------------------------------------------------------------------------

//----------------------------------------------------------------------------------------------------------------------
namespace Startup {
//----------------------------------------------------------------------------------------------------------------------

enum class ParseCode : std::uint32_t { Malformed, ExitRequested, Success };

class Options;

//----------------------------------------------------------------------------------------------------------------------
} // Option namespace
//----------------------------------------------------------------------------------------------------------------------

class Startup::Options
{
public:
    static constexpr std::string_view Help = "help";
    static constexpr std::string_view Version = "version";
    static constexpr std::string_view Verbosity = "verbosity";
    static constexpr std::string_view Quiet = "quiet";
    static constexpr std::string_view NonInteractive = "non-interactive";
    static constexpr std::string_view Configuration = "config";
    static constexpr std::string_view Peers = "peers";
    static constexpr std::string_view DisableBootstrap = "disable-bootstrap";

    Options();

    void SetupDescriptions();
    [[nodiscard]] ParseCode Parse(std::int32_t argc, char** argv);

    [[nodiscard]] std::string GenerateHelpText(std::int32_t argc, char** argv);
    [[nodiscard]] std::string GenerateVersionText(std::int32_t argc, char** argv);

    [[nodiscard]] spdlog::level::level_enum GetVerbosityLevel() const;
    [[nodiscard]] bool IsInteractive() const;
    [[nodiscard]] std::string const& GetConfigurationPath() const;
    [[nodiscard]] std::string const& GetPeersPath() const;
    [[nodiscard]] bool UseBootstraps() const;

private:
    using VerbosityLevels = std::vector<std::pair<std::string, spdlog::level::level_enum>>;

    boost::program_options::options_description m_descriptions;
    boost::program_options::variables_map m_options;
    VerbosityLevels m_levels;

    spdlog::level::level_enum m_verbosity;
    bool m_interactive;
    std::string m_config;
    std::string m_peers;
    bool m_bootstrap;
};

//----------------------------------------------------------------------------------------------------------------------
