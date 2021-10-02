//----------------------------------------------------------------------------------------------------------------------
// File: Options.hpp
// Description:
//----------------------------------------------------------------------------------------------------------------------
#pragma once
//----------------------------------------------------------------------------------------------------------------------
#include "BryptIdentifier/BryptIdentifier.hpp"
#include "BryptNode/RuntimeContext.hpp"
#include "Components/Network/Address.hpp"
#include "Components/Network/Protocol.hpp"
#include "Components/Security/SecurityDefinitions.hpp"
#include "Components/Security/SecurityUtils.hpp"
#include "Utilities/Version.hpp"
//----------------------------------------------------------------------------------------------------------------------
#include <spdlog/common.h>
//----------------------------------------------------------------------------------------------------------------------
#include <chrono>
#include <compare>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>
//----------------------------------------------------------------------------------------------------------------------

namespace spdlog { class logger; }


//----------------------------------------------------------------------------------------------------------------------
namespace Configuration {
//----------------------------------------------------------------------------------------------------------------------

std::filesystem::path const DefaultBryptFolder = "/brypt/";
std::filesystem::path const DefaultConfigurationFilename = "config.json";
std::filesystem::path const DefaultBootstrapFilename = "bootstrap.json";

std::filesystem::path GetDefaultBryptFolder();
std::filesystem::path GetDefaultConfigurationFilepath();
std::filesystem::path GetDefaultBootstrapFilepath();

//----------------------------------------------------------------------------------------------------------------------
namespace Options {
//----------------------------------------------------------------------------------------------------------------------

struct Runtime;
struct Identifier;
struct Details;
struct Connection;
struct Endpoint;
struct Network;
struct Security;

using Endpoints = std::vector<Endpoint>;

//----------------------------------------------------------------------------------------------------------------------
} // Options namespace
//----------------------------------------------------------------------------------------------------------------------
} // Configuration namespace
//----------------------------------------------------------------------------------------------------------------------

//----------------------------------------------------------------------------------------------------------------------
// Description: The set of options defined at runtime (e.g. cli flags or or shared library methods).
//----------------------------------------------------------------------------------------------------------------------
struct Configuration::Options::Runtime
{
    RuntimeContext context;
    spdlog::level::level_enum verbosity;
    bool useInteractiveConsole;
    bool useBootstraps;
    bool useFilepathDeduction;
};

//----------------------------------------------------------------------------------------------------------------------
// Description: The set of options defined at runtime (e.g. cli flags or or shared library methods).
//----------------------------------------------------------------------------------------------------------------------
struct Configuration::Options::Identifier
{
    enum Type : std::uint32_t { Invalid, Ephemeral, Persistent};
    struct ConstructedValues { Type type; Node::SharedIdentifier value; };

    Identifier();
    explicit Identifier(std::string_view type, std::string_view value = "");
    [[nodiscard]] std::strong_ordering operator<=>(Identifier const& other) const noexcept;
    [[nodiscard]] bool operator==(Identifier const& other) const noexcept;

    void Merge(Identifier& other);
    [[nodiscard]] bool Initialize(std::shared_ptr<spdlog::logger> const& logger);

    std::string type;
    std::optional<std::string> value;

    ConstructedValues constructed;
};

//----------------------------------------------------------------------------------------------------------------------
// Description: The set of options used to establish an identifier for the brypt node. 
//----------------------------------------------------------------------------------------------------------------------
struct Configuration::Options::Details
{
    Details();
    explicit Details(std::string_view name, std::string_view description = "", std::string_view location = "");

    void Merge(Details& other);
    [[nodiscard]] bool Initialize(std::shared_ptr<spdlog::logger> const& logger);

    std::string name;
    std::string description;
    std::string location;
};

//----------------------------------------------------------------------------------------------------------------------
// Description: The set of options used for connections. 
//----------------------------------------------------------------------------------------------------------------------
struct Configuration::Options::Connection
{
    struct ConstructedValues { 
        std::chrono::milliseconds timeout;
        std::chrono::milliseconds interval;
    };

    struct Retry {
        [[nodiscard]] std::strong_ordering operator<=>(Retry const& other) const noexcept;
        [[nodiscard]] bool operator==(Retry const& other) const noexcept;

        std::int32_t limit;
        std::string interval;
    };

    Connection();
    Connection(
        std::chrono::milliseconds const& _timeout, std::int32_t _limit, std::chrono::milliseconds const& _interval);

    [[nodiscard]] std::strong_ordering operator<=>(Connection const& other) const noexcept;
    [[nodiscard]] bool operator==(Connection const& other) const noexcept;

    void Merge(Connection& other);
    void Merge(Connection const& other);
    [[nodiscard]] bool Initialize(std::shared_ptr<spdlog::logger> const& logger);

    [[nodiscard]] std::chrono::milliseconds const& GetTimeout() const;
    [[nodiscard]] std::int32_t GetRetryLimit() const;
    [[nodiscard]] std::chrono::milliseconds const& GetRetryInterval() const;

    [[nodiscard]] bool SetTimeout(std::chrono::milliseconds const& value);
    [[nodiscard]] bool SetRetryLimit(std::int32_t value);
    [[nodiscard]] bool SetRetryInterval(std::chrono::milliseconds const& value);

    std::string timeout;
    Retry retry;

    ConstructedValues constructed;
};

//----------------------------------------------------------------------------------------------------------------------
// Description: The set of options used to create an endpoint. 
//----------------------------------------------------------------------------------------------------------------------
struct Configuration::Options::Endpoint
{
    struct ConstructedValues { 
        ::Network::Protocol protocol;
        ::Network::BindingAddress binding;
        std::optional<::Network::RemoteAddress> bootstrap;
    };

    Endpoint();
    Endpoint(
        ::Network::Protocol protocol,
        std::string_view interface,
        std::string_view binding,
        std::optional<std::string> const& bootstrap = {});
    Endpoint(
        std::string_view protocol,
        std::string_view interface,
        std::string_view binding,
        std::optional<std::string> const& bootstrap = {});

    [[nodiscard]] std::strong_ordering operator<=>(Endpoint const& other) const noexcept;
    [[nodiscard]] bool operator==(Endpoint const& other) const noexcept;

    void Merge(Endpoint& other);
    [[nodiscard]] bool Initialize(std::shared_ptr<spdlog::logger> const& logger);

    [[nodiscard]] ::Network::Protocol GetProtocol() const;
    [[nodiscard]] std::string const& GetProtocolString() const;
    [[nodiscard]] std::string const& GetInterface() const;
    [[nodiscard]] ::Network::BindingAddress const& GetBinding() const;
    [[nodiscard]] std::optional<::Network::RemoteAddress> const& GetBootstrap() const;
    [[nodiscard]] std::chrono::milliseconds const& GetConnectionTimeout() const;
    [[nodiscard]] std::int32_t GetConnectionRetryLimit() const;
    [[nodiscard]] std::chrono::milliseconds const& GetConnectionRetryInterval() const;

    [[nodiscard]] bool SetConnectionRetryLimit(std::int32_t limit);
    [[nodiscard]] bool SetConnectionTimeout(std::chrono::milliseconds const& timeout);
    [[nodiscard]] bool SetConnectionRetryInterval(std::chrono::milliseconds const& interval);
    void SetConnectionOptions(Connection const& options);

    std::string protocol;
    std::string interface;
    std::string binding;
    std::optional<std::string> bootstrap;
    std::optional<Connection> connection;

    ConstructedValues constructed;
};

//----------------------------------------------------------------------------------------------------------------------
// Description: The set of options used to configure the network. 
//----------------------------------------------------------------------------------------------------------------------
struct Configuration::Options::Network
{
    Network();

    [[nodiscard]] std::strong_ordering operator<=>(Network const& other) const noexcept;
    [[nodiscard]] bool operator==(Network const& other) const noexcept;

    void Merge(Network& other);
    [[nodiscard]] bool Initialize(std::shared_ptr<spdlog::logger> const& logger);

    [[nodiscard]] Endpoints const& GetEndpoints() const;
    [[nodiscard]] std::chrono::milliseconds const& GetConnectionTimeout() const;
    [[nodiscard]] std::int32_t GetConnectionRetryLimit() const;
    [[nodiscard]] std::chrono::milliseconds const& GetConnectionRetryInterval() const;

    [[nodiscard]] bool SetConnectionRetryLimit(std::int32_t limit);
    [[nodiscard]] bool SetConnectionTimeout(std::chrono::milliseconds const& timeout);
    [[nodiscard]] bool SetConnectionRetryInterval(std::chrono::milliseconds const& interval);

    Endpoints endpoints;
    Connection connection;
};

//----------------------------------------------------------------------------------------------------------------------
// Description: The set of options used to configure the security strategy. 
//----------------------------------------------------------------------------------------------------------------------
struct Configuration::Options::Security
{
    struct ConstructedValues { ::Security::Strategy strategy; };

    Security();
    Security(std::string_view strategy, std::string_view token);
    [[nodiscard]] std::strong_ordering operator<=>(Security const& other) const noexcept;
    [[nodiscard]] bool operator==(Security const& other) const noexcept;

    void Merge(Security& other);
    [[nodiscard]] bool Initialize(std::shared_ptr<spdlog::logger> const& logger);

    std::string strategy;
    std::optional<std::string> token;

    ConstructedValues constructed;
};

//----------------------------------------------------------------------------------------------------------------------
