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
#include "Utilities/InvokeContext.hpp"
#include "Utilities/Version.hpp"
//----------------------------------------------------------------------------------------------------------------------
#include <boost/json/fwd.hpp>
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

constexpr std::string_view DefaultBryptFolder = "/brypt/";

std::filesystem::path const DefaultConfigurationFilename = "config.json";
std::filesystem::path const DefaultBootstrapFilename = "bootstrap.json";

std::filesystem::path GetDefaultBryptFolder();
std::filesystem::path GetDefaultConfigurationFilepath();
std::filesystem::path GetDefaultBootstrapFilepath();

//----------------------------------------------------------------------------------------------------------------------
namespace Options {
//----------------------------------------------------------------------------------------------------------------------

struct Runtime;
class Identifier;
class Details;
class Connection;
class Endpoint;
class Network;
class Security;

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
class Configuration::Options::Identifier
{
public:
    static constexpr std::string_view Symbol = "identifier";

    enum Type : std::uint32_t { Invalid, Ephemeral, Persistent};

    Identifier();
    explicit Identifier(std::string_view type, std::string_view value = "");
    [[nodiscard]] std::strong_ordering operator<=>(Identifier const& other) const noexcept;
    [[nodiscard]] bool operator==(Identifier const& other) const noexcept;

    void Merge(Identifier& other);
    void Merge(boost::json::object const& json);
    void Write(boost::json::object& json) const;
    [[nodiscard]] bool Initialize(std::shared_ptr<spdlog::logger> const& logger);
    [[nodiscard]] bool IsAllowable() const;

    [[nodiscard]] Type GetType() const;
    [[nodiscard]] Node::SharedIdentifier const& GetValue() const;

    [[nodiscard]] bool SetIdentifier(Type type, bool& changed, std::shared_ptr<spdlog::logger> const& logger);

private:
    struct ConstructedValues { 
        Type type;
        Node::SharedIdentifier value;
    };

    std::string m_type;
    std::optional<std::string> m_optValue;

    ConstructedValues m_constructed;
};

//----------------------------------------------------------------------------------------------------------------------
// Description: The set of options used to establish an identifier for the brypt node. 
//----------------------------------------------------------------------------------------------------------------------
class Configuration::Options::Details
{
public:
    static constexpr std::string_view Symbol = "details";

    Details();
    explicit Details(std::string_view name, std::string_view description = "", std::string_view location = "");

    void Merge(Details& other);
    void Merge(boost::json::object const& json);
    void Write(boost::json::object& json) const;
    [[nodiscard]] bool Initialize(std::shared_ptr<spdlog::logger> const& logger);

    [[nodiscard]] std::string const& GetName() const;
    [[nodiscard]] std::string const& GetDescription() const;
    [[nodiscard]] std::string const& GetLocation() const;

    [[nodiscard]] bool SetName(std::string_view const& name, bool& changed);
    [[nodiscard]] bool SetDescription(std::string_view const& name, bool& changed);
    [[nodiscard]] bool SetLocation(std::string_view const& name, bool& changed);

private:
    std::string m_name;
    std::string m_description;
    std::string m_location;
};

//----------------------------------------------------------------------------------------------------------------------
// Description: The set of options used for connections. 
//----------------------------------------------------------------------------------------------------------------------
class Configuration::Options::Connection
{
public:
    static constexpr std::string_view Symbol = "connection";
    using GlobalOptionsReference = std::optional<std::reference_wrapper<Connection const>>;

    Connection();
    Connection(
        std::chrono::milliseconds const& _timeout, std::int32_t _limit, std::chrono::milliseconds const& _interval);

    [[nodiscard]] std::strong_ordering operator<=>(Connection const& other) const noexcept;
    [[nodiscard]] bool operator==(Connection const& other) const noexcept;

    void Merge(Connection& other);
    void Merge(Connection const& other);
    void Merge(boost::json::object const& json);
    void Write(
        boost::json::object& json, GlobalOptionsReference optGlobalConnectionOptions = {}) const;
    [[nodiscard]] bool Initialize(std::shared_ptr<spdlog::logger> const& logger);

    [[nodiscard]] std::chrono::milliseconds const& GetTimeout() const;
    [[nodiscard]] std::int32_t GetRetryLimit() const;
    [[nodiscard]] std::chrono::milliseconds const& GetRetryInterval() const;

    [[nodiscard]] bool SetTimeout(std::chrono::milliseconds const& value, bool& changed);
    [[nodiscard]] bool SetRetryLimit(std::int32_t value, bool& changed);
    [[nodiscard]] bool SetRetryInterval(std::chrono::milliseconds const& value, bool& changed);

private:
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

    std::string m_timeout;
    Retry m_retry;

    ConstructedValues m_constructed;
};

//----------------------------------------------------------------------------------------------------------------------
// Description: The set of options used to create an endpoint. 
//----------------------------------------------------------------------------------------------------------------------
class Configuration::Options::Endpoint
{
public:
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

    Endpoint(boost::json::object const& json);

    [[nodiscard]] std::strong_ordering operator<=>(Endpoint const& other) const noexcept;
    [[nodiscard]] bool operator==(Endpoint const& other) const noexcept;

    void Merge(Endpoint& other);
    void Merge(boost::json::object const& json);
    void Write(
        boost::json::object& json, Connection::GlobalOptionsReference optGlobalConnectionOptions = {}) const;
    [[nodiscard]] bool Initialize(Runtime runtime, std::shared_ptr<spdlog::logger> const& logger);

    [[nodiscard]] ::Network::Protocol GetProtocol() const;
    [[nodiscard]] std::string const& GetProtocolString() const;
    [[nodiscard]] std::string const& GetInterface() const;
    [[nodiscard]] std::string const& GetBindingField() const;
    [[nodiscard]] ::Network::BindingAddress const& GetBinding() const;
    [[nodiscard]] std::optional<::Network::RemoteAddress> const& GetBootstrap() const;
    [[nodiscard]] std::optional<std::string> const& GetBootstrapField() const;
    [[nodiscard]] bool UseBootstraps() const;
    [[nodiscard]] std::chrono::milliseconds const& GetConnectionTimeout() const;
    [[nodiscard]] std::int32_t GetConnectionRetryLimit() const;
    [[nodiscard]] std::chrono::milliseconds const& GetConnectionRetryInterval() const;

    [[nodiscard]] bool SetConnectionRetryLimit(std::int32_t limit, bool& changed);
    [[nodiscard]] bool SetConnectionTimeout(std::chrono::milliseconds const& timeout, bool& changed);
    [[nodiscard]] bool SetConnectionRetryInterval(std::chrono::milliseconds const& interval, bool& changed);
    void UpsertConnectionOptions(Connection const& options);

    UT_SupportMethod(static Endpoint CreateTestOptions(::Network::BindingAddress const& _binding));
    UT_SupportMethod(static Endpoint CreateTestOptions(std::string_view _interface, std::string_view _binding));

private:
    struct ConstructedValues {
        ::Network::Protocol protocol;
        ::Network::BindingAddress binding;
        std::optional<::Network::RemoteAddress> bootstrap;
    };

    struct TransientValues {
        bool useBootstraps;
    };

    std::string m_protocol;
    std::string m_interface;
    std::string m_binding;
    std::optional<std::string> m_optBootstrap;
    std::optional<Connection> m_optConnection;

    ConstructedValues m_constructed;
    TransientValues m_transient;
};

//----------------------------------------------------------------------------------------------------------------------
// Description: The set of options used to configure the network. 
//----------------------------------------------------------------------------------------------------------------------
class Configuration::Options::Network
{
public:
    static constexpr std::string_view Symbol = "network";

    using FetchedEndpoint = std::optional<std::reference_wrapper<Options::Endpoint const>>;

    Network();

    [[nodiscard]] std::strong_ordering operator<=>(Network const& other) const noexcept;
    [[nodiscard]] bool operator==(Network const& other) const noexcept;

    void Merge(Network& other);
    void Merge(boost::json::object const& json);
    void Write(boost::json::object& json) const;
    [[nodiscard]] bool Initialize(Runtime const& runtime, std::shared_ptr<spdlog::logger> const& logger);
    [[nodiscard]] bool IsAllowable(Runtime const& runtime) const;

    [[nodiscard]] Endpoints const& GetEndpoints() const;
    [[nodiscard]] FetchedEndpoint GetEndpoint(::Network::BindingAddress const& binding) const;
    [[nodiscard]] FetchedEndpoint GetEndpoint(std::string_view const& uri) const;
    [[nodiscard]] FetchedEndpoint GetEndpoint(::Network::Protocol protocol, std::string_view const& binding) const;
    [[nodiscard]] std::chrono::milliseconds const& GetConnectionTimeout() const;
    [[nodiscard]] std::int32_t GetConnectionRetryLimit() const;
    [[nodiscard]] std::chrono::milliseconds const& GetConnectionRetryInterval() const;
    [[nodiscard]] std::optional<std::string> const& GetToken() const;

    [[nodiscard]] FetchedEndpoint UpsertEndpoint(
        Endpoint&& options, Runtime const& runtime, std::shared_ptr<spdlog::logger> const& logger, bool& changed);
    [[nodiscard]] std::optional<Endpoint> ExtractEndpoint(::Network::BindingAddress const& binding, bool& changed);
    [[nodiscard]] std::optional<Endpoint> ExtractEndpoint(std::string_view const& uri, bool& changed);
    [[nodiscard]] std::optional<Endpoint> ExtractEndpoint(
        ::Network::Protocol protocol, std::string_view const& binding, bool& changed);
    [[nodiscard]] bool SetConnectionRetryLimit(std::int32_t limit, bool& changed);
    [[nodiscard]] bool SetConnectionTimeout(std::chrono::milliseconds const& timeout, bool& changed);
    [[nodiscard]] bool SetConnectionRetryInterval(std::chrono::milliseconds const& interval, bool& changed);
    [[nodiscard]] bool SetToken(std::string_view const& token, bool& changed);

private:
    Endpoints m_endpoints;
    Connection m_connection;
    std::optional<std::string> m_optToken;
};

//----------------------------------------------------------------------------------------------------------------------
// Description: The set of options used to configure the security strategy. 
//----------------------------------------------------------------------------------------------------------------------
class Configuration::Options::Security
{
public:
    static constexpr std::string_view Symbol = "security";

    Security();
    explicit Security(std::string_view strategy);
    [[nodiscard]] std::strong_ordering operator<=>(Security const& other) const noexcept;
    [[nodiscard]] bool operator==(Security const& other) const noexcept;

    void Merge(Security& other);
    void Merge(boost::json::object const& json);
    void Write(boost::json::object& json) const;
    [[nodiscard]] bool Initialize(std::shared_ptr<spdlog::logger> const& logger);
    [[nodiscard]] bool IsAllowable() const;

    [[nodiscard]] ::Security::Strategy GetStrategy() const;
    void SetStrategy(::Security::Strategy strategy, bool& changed);

private:
    struct ConstructedValues { ::Security::Strategy strategy; };
    
    std::string m_strategy;

    ConstructedValues m_constructed;
};

//----------------------------------------------------------------------------------------------------------------------
