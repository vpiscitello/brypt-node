//----------------------------------------------------------------------------------------------------------------------
#include "StartupOptions.hpp"
#include "Components/Configuration/Options.hpp"
#include "Utilities/Logger.hpp"
//----------------------------------------------------------------------------------------------------------------------
#include <algorithm>
#include <cstdint>
#include <iostream>
#include <optional>
#if defined(WIN32)
#include <windows.h>
#else
#include <sys/ioctl.h>
#endif
//----------------------------------------------------------------------------------------------------------------------

//----------------------------------------------------------------------------------------------------------------------
namespace {
namespace local {
//----------------------------------------------------------------------------------------------------------------------

std::uint32_t GetTerminalWidth();

//----------------------------------------------------------------------------------------------------------------------
} // local namespace
} // namespace
//----------------------------------------------------------------------------------------------------------------------

Startup::Options::Options()
    : m_descriptions()
    , m_options()
    , m_levels()
    , m_verbosity(spdlog::level::info)
    , m_interactive(true)
    , m_configurationFilepath()
    , m_bootstrapFilepath()
    , m_useBootstraps(true)
{
    SetupDescriptions();
}

//----------------------------------------------------------------------------------------------------------------------

void Startup::Options::SetupDescriptions()
{
    std::uint32_t const width = local::GetTerminalWidth();
    boost::program_options::options_description general("General Options", width);
    auto AddGeneralOption = general.add_options();

    AddGeneralOption(Help.data(), "Display this help text and exit.");
    AddGeneralOption(Version.data(), "Display the version information and exit.");

    // Option to set the log verbosity level.
    {
        m_levels = {
            { "trace", spdlog::level::trace },
            { "debug", spdlog::level::debug },
            { "info", spdlog::level::info },
            { "warning", spdlog::level::warn },
            { "error", spdlog::level::err },
            { "critical", spdlog::level::critical },
            { "none", spdlog::level::off },
        };
        
        std::ostringstream oss;
        oss << "Sets the maximum log level for console output. ";
        oss << "Options: [";
        std::size_t idx = 0;
        for (auto const& [name, value] : m_levels) {
            oss << name << ((++idx < m_levels.size()) ? ", " : "");
        }
        oss << "]";
        AddGeneralOption(
            Verbosity.data(),
            boost::program_options::value<std::string>()->value_name(
                "<level>")->default_value("info"),
            oss.str().c_str());
    }

    // Option to disable console output.
    {
        AddGeneralOption(
            Quiet.data(),
            boost::program_options::bool_switch()->default_value(false),
            "Disables all output to the console and enables non-interactive mode. "
            "If input is required, an error will be raised instead.");
    }

    // Option to disable console input.
    {
        AddGeneralOption(
            NonInteractive.data(),
            boost::program_options::bool_switch()->default_value(false),
            "Disables all interactive input prompts. If input is required,""an error will be "
            "raised instead.");
    }

    m_descriptions.add(general);

    boost::program_options::options_description configuration("Configuration Options", width);
    auto AddConfigurationOption = configuration.add_options();

    // Option to set the configuration filepath.
    {
        auto const filepath = Configuration::GetDefaultConfigurationFilepath();
        std::ostringstream oss;
        oss << "Set the configuration filepath. This may specify a complete filepath or ";
        oss << "directory. If a directory is specified \"config.json\" is assumed. ";
        oss << "If a directory is not specified, the default configuration folder will be used.";
        AddConfigurationOption(
            ConfigurationFilepath.data(),
            boost::program_options::value(
                &m_configurationFilepath)->value_name("<filepath>")->default_value(
                    filepath.string()), oss.str().c_str());
    }

    // Option to set the Peer Persistance filepath.
    {
        auto const filepath = Configuration::GetDefaultBootstrapFilepath();
        std::ostringstream oss;
        oss << "Set the bootstrap filepath. This may specify a complete filepath or ";
        oss << "directory. If a directory is specified \"bootstrap.json\" is assumed. ";
        oss << "If a directory is not specified, the default configuration folder will be used.";
        AddConfigurationOption(
            BootstrapFilepath.data(),
            boost::program_options::value(
                &m_bootstrapFilepath)->value_name("<filepath>")->default_value(
                    filepath.string()), oss.str().c_str());
    }

    // Option to disable initial bootstrapping.
    {
        AddConfigurationOption(
            DisableBootstrap.data(),
            "Disables bootstrapping to addresses enumerated in the bootstrap file.");
    }

    m_descriptions.add(configuration);
}

//----------------------------------------------------------------------------------------------------------------------

Startup::ParseCode Startup::Options::Parse(std::int32_t argc, char** argv)
{
    constexpr auto IsOptionSupplied = [] (
        boost::program_options::variables_map const& options,
        std::string_view option) -> bool
    {
        return options.count(option.data()) && !options[option.data()].defaulted();
    };

    constexpr auto CheckConflictingOptions = [] (
        boost::program_options::variables_map const& options,
        std::string_view left,
        std::string_view right) -> std::optional<std::string>
    {
        if (options.count(left.data()) && !options[left.data()].defaulted() && 
            options.count(right.data()) && !options[right.data()].defaulted()) {
            std::ostringstream oss;
            oss << "Conflicting options '" << left << "' and '" << right << "'.";
            return oss.str();
        }
        return {};
    };

    try {
        boost::program_options::store(
            boost::program_options::command_line_parser(argc, argv).options(m_descriptions).run(), m_options);
    } catch (std::exception& e) {
        std::cout << "An error occured parsing startup options due to: ";
        std::cout << e.what() << "." << std::endl;
        return ParseCode::Malformed;
    }

    boost::program_options::notify(m_options);

    if(IsOptionSupplied(m_options, Help)) {
        std::cout << GenerateHelpText(argc, argv) << std::endl;
        return ParseCode::ExitRequested;
    }

    if(IsOptionSupplied(m_options, Version)) {
        std::cout << GenerateVersionText(argc, argv) << std::endl;
        return ParseCode::ExitRequested;
    }

    if(IsOptionSupplied(m_options, Verbosity)) {
        auto const& argument = m_options[Verbosity.data()].as<std::string>();
        VerbosityLevels::iterator const itr = std::ranges::find_if(m_levels, [&argument] (auto const& item) -> bool {
            return (argument == item.first);
        });

        if (itr == m_levels.end()) {
            std::cout << "Unreconized verbosity level!" << std::endl;
            return ParseCode::Malformed;
        }

        m_verbosity = itr->second;
    }

    if(IsOptionSupplied(m_options, Quiet)) { m_verbosity = spdlog::level::off; }
    if(IsOptionSupplied(m_options, NonInteractive)) { m_interactive = false; }
    if(IsOptionSupplied(m_options, DisableBootstrap)) { m_useBootstraps = false; }

    if (m_configurationFilepath.empty()) { 
        std::cout << "The configuration filepath cannot be empty." << std::endl;
        return ParseCode::Malformed;
    }

    if (m_bootstrapFilepath.empty()) { 
        std::cout << "The boostrap filepath cannot be empty." << std::endl;
        return ParseCode::Malformed;
    }

    if (auto const optError = CheckConflictingOptions(m_options, Verbosity, Quiet); optError) {
        std::cout << *optError << std::endl;
        return ParseCode::Malformed;
    }

    if (auto const optError = CheckConflictingOptions(m_options, NonInteractive, Quiet); optError) {
        std::cout << *optError << std::endl;
        return ParseCode::Malformed;
    }

    return ParseCode::Success;
}

//----------------------------------------------------------------------------------------------------------------------

std::string Startup::Options::GenerateHelpText([[maybe_unused]] std::int32_t argc, char** argv)
{   
    std::ostringstream oss;
    std::string name = std::filesystem::path(argv[0]).stem().string();
    oss << "Usage: " << name << " [options] \n" << m_descriptions;
    return oss.str();
}

//----------------------------------------------------------------------------------------------------------------------

std::string Startup::Options::GenerateVersionText([[maybe_unused]] std::int32_t argc, char** argv)
{   
    std::ostringstream oss;
    std::string name = std::filesystem::path(argv[0]).stem().string();
    oss << name << " (Brypt Node) " << Brypt::Version;
    return oss.str();
}

//----------------------------------------------------------------------------------------------------------------------

spdlog::level::level_enum Startup::Options::GetVerbosity() const
{
    return m_verbosity;
}

//----------------------------------------------------------------------------------------------------------------------

bool Startup::Options::IsInteractive() const
{
    return m_interactive;
}

//----------------------------------------------------------------------------------------------------------------------

std::string const& Startup::Options::GetConfigPath() const
{
    return m_configurationFilepath;
}

//----------------------------------------------------------------------------------------------------------------------

std::string const& Startup::Options::GetBootstrapPath() const
{
    return m_bootstrapFilepath;
}

//----------------------------------------------------------------------------------------------------------------------

bool Startup::Options::UseBootstraps() const
{
    return m_useBootstraps;
}

//----------------------------------------------------------------------------------------------------------------------

Startup::Options::operator Configuration::Options::Runtime() const
{
    // Package the parsed command line options to the runtime options aggregate.
    return {
        .context = RuntimeContext::Foreground,
        .verbosity = m_verbosity,
        .useInteractiveConsole = m_interactive,
        .useBootstraps = m_useBootstraps,
        .useFilepathDeduction = true
    };
}

//----------------------------------------------------------------------------------------------------------------------

std::uint32_t local::GetTerminalWidth()
{
#if defined(WIN32)
    CONSOLE_SCREEN_BUFFER_INFO info;
    GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE), &info);
    return static_cast<std::uint32_t>(info.srWindow.Right - info.srWindow.Left + 1);
#else
    struct winsize size;
    ::ioctl(::fileno(::stdout), TIOCGWINSZ, &size);
    return static_cast<std::uint32_t>(size.ws_col);
#endif
}

//----------------------------------------------------------------------------------------------------------------------
