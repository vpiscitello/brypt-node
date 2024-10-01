//----------------------------------------------------------------------------------------------------------------------
// File: Options.hpp
// Description:
//----------------------------------------------------------------------------------------------------------------------
#pragma once
//----------------------------------------------------------------------------------------------------------------------
#include "Field.hpp"
#include "StatusCode.hpp"
#include "Components/Core/RuntimeContext.hpp"
#include "Components/Identifier/BryptIdentifier.hpp"
#include "Components/Network/Address.hpp"
#include "Components/Network/Protocol.hpp"
#include "Components/Security/SecurityDefinitions.hpp"
#include "Components/Security/SecurityUtils.hpp"
#include "Utilities/CallbackIteration.hpp"
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
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>
//----------------------------------------------------------------------------------------------------------------------

//----------------------------------------------------------------------------------------------------------------------
namespace Configuration {
//----------------------------------------------------------------------------------------------------------------------

constexpr std::string_view DefaultBryptFolder = "/brypt/";

std::filesystem::path const DefaultConfigurationFilename = "config.json";
std::filesystem::path const DefaultBootstrapFilename = "bootstraps.json";

std::filesystem::path GetDefaultBryptFolder();
std::filesystem::path GetDefaultConfigurationFilepath();
std::filesystem::path GetDefaultBootstrapFilepath();

//----------------------------------------------------------------------------------------------------------------------
namespace Options {
//----------------------------------------------------------------------------------------------------------------------

struct Runtime;

class Identifier;
class Details;
class Retry;
class Connection;
class Endpoint;
class Network;
class Algorithms;
class SupportedAlgorithms;
class Security;

using GlobalConnectionOptionsReference = std::optional<std::reference_wrapper<Connection const>>;

using Endpoints = std::vector<Endpoint>;

//----------------------------------------------------------------------------------------------------------------------
} // Options namespace
//----------------------------------------------------------------------------------------------------------------------
namespace Symbols {
//----------------------------------------------------------------------------------------------------------------------

DEFINE_FIELD_NAME(Algorithms);
DEFINE_FIELD_NAME(Binding);
DEFINE_FIELD_NAME(Bootstrap);
DEFINE_FIELD_NAME(Ciphers);
DEFINE_FIELD_NAME(Connection);
DEFINE_FIELD_NAME(Description);
DEFINE_FIELD_NAME(Details);
DEFINE_FIELD_NAME(Endpoints);
DEFINE_FIELD_NAME(HashFunctions);
DEFINE_FIELD_NAME(Identifier);
DEFINE_FIELD_NAME(Interface);
DEFINE_FIELD_NAME(Interval);
DEFINE_FIELD_NAME(KeyAgreements);
DEFINE_FIELD_NAME(Limit);
DEFINE_FIELD_NAME(Location);
DEFINE_FIELD_NAME(Name);
DEFINE_FIELD_NAME(Network);
DEFINE_FIELD_NAME(Persistence);
DEFINE_FIELD_NAME(Protocol);
DEFINE_FIELD_NAME(Retry);
DEFINE_FIELD_NAME(Security);
DEFINE_FIELD_NAME(Strategy);
DEFINE_FIELD_NAME(Timeout);
DEFINE_FIELD_NAME(Token);
DEFINE_FIELD_NAME(Type);
DEFINE_FIELD_NAME(Value);

//----------------------------------------------------------------------------------------------------------------------
} // Symbols namespace
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
// Description: The set of options used to establish an identifier for the brypt node. 
//----------------------------------------------------------------------------------------------------------------------
class Configuration::Options::Identifier
{
public:
    static constexpr std::string_view Symbol = Symbols::Identifier{};

    enum class Persistence : std::uint32_t { Invalid, Ephemeral, Persistent };

    Identifier();
    explicit Identifier(std::string_view persistence, std::string_view value = "");

    [[nodiscard]] bool operator==(Identifier const& other) const noexcept;
    [[nodiscard]] std::strong_ordering operator<=>(Identifier const& other) const noexcept;

    [[nodiscard]] static constexpr std::string_view GetFieldName() { return Symbol; }
    [[nodiscard]] static constexpr bool IsOptional(){ return false; }

    [[nodiscard]] DeserializationResult Merge(boost::json::object const& json);
    [[nodiscard]] SerializationResult Write(boost::json::object& json) const;

    [[nodiscard]] ValidationResult AreOptionsAllowable() const;

    [[nodiscard]] Persistence GetPersistence() const;
    [[nodiscard]] Node::SharedIdentifier const& GetValue() const;

    [[nodiscard]] bool SetIdentifier(Persistence persistance, bool& changed);

private:
    ConstructedField<Symbols::Persistence, Persistence> m_persistence;
    OptionalConstructedField<Symbols::Value, Node::SharedIdentifier> m_optValue;
};

//----------------------------------------------------------------------------------------------------------------------
// Description: The set of options used to describe some details about the node.
//----------------------------------------------------------------------------------------------------------------------
class Configuration::Options::Details
{
public:
    static constexpr std::string_view Symbol = Symbols::Details{};
    static constexpr std::size_t NameSizeLimit = 256;
    static constexpr std::size_t DescriptionSizeLimit = 1024;
    static constexpr std::size_t LocationSizeLimit = 1024;

    Details();
    explicit Details(std::string_view name, std::string_view description = "", std::string_view location = "");

    [[nodiscard]] bool operator==(Details const& other) const noexcept;
    [[nodiscard]] std::strong_ordering operator<=>(Details const& other) const noexcept;

    [[nodiscard]] static constexpr std::string_view GetFieldName() { return Symbol; }
    [[nodiscard]] static constexpr bool IsOptional() { return true; }

    [[nodiscard]] DeserializationResult Merge(boost::json::object const& json);
    [[nodiscard]] SerializationResult Write(boost::json::object& json) const;

    [[nodiscard]] ValidationResult AreOptionsAllowable() const;

    [[nodiscard]] std::string const& GetName() const;
    [[nodiscard]] std::string const& GetDescription() const;
    [[nodiscard]] std::string const& GetLocation() const;

    [[nodiscard]] bool SetName(std::string_view const& name, bool& changed);
    [[nodiscard]] bool SetDescription(std::string_view const& name, bool& changed);
    [[nodiscard]] bool SetLocation(std::string_view const& name, bool& changed);

private:
    OptionalField<Symbols::Name, std::string> m_optName;
    OptionalField<Symbols::Description, std::string> m_optDescription;
    OptionalField<Symbols::Location, std::string> m_optLocation;
};

//----------------------------------------------------------------------------------------------------------------------
// Description: The set of options used for retry. 
//----------------------------------------------------------------------------------------------------------------------
class Configuration::Options::Retry
{
public:
    static constexpr std::string_view Symbol = Symbols::Retry{};

    Retry();
    Retry(std::uint32_t limit, std::chrono::milliseconds const& interval);

    [[nodiscard]] bool operator==(Retry const& other) const noexcept;
    [[nodiscard]] std::strong_ordering operator<=>(Retry const& other) const noexcept;

    [[nodiscard]] static constexpr std::string_view GetFieldName() { return Symbol; }
    [[nodiscard]] static constexpr bool IsOptional() { return true; }

    [[nodiscard]] DeserializationResult Merge(Retry& other);
    [[nodiscard]] DeserializationResult Merge(
        boost::json::object const& json,
        std::string_view context = "",
        GlobalConnectionOptionsReference optGlobalOptions = {});
    [[nodiscard]] SerializationResult Write(
        boost::json::object& json, GlobalConnectionOptionsReference optGlobalOptions = {}) const;

    [[nodiscard]] ValidationResult AreOptionsAllowable() const;

    [[nodiscard]] std::uint32_t GetLimit() const;
    [[nodiscard]] std::chrono::milliseconds const& GetInterval() const;

    [[nodiscard]] bool SetLimit(std::int32_t value, bool& changed);
    [[nodiscard]] bool SetLimit(std::uint32_t value, bool& changed);
    [[nodiscard]] bool SetInterval(std::chrono::milliseconds const& value, bool& changed);

private:
    OptionalField<Symbols::Limit, std::uint32_t> m_optLimit;
    OptionalConstructedField<Symbols::Interval, std::chrono::milliseconds> m_optInterval;
};

//----------------------------------------------------------------------------------------------------------------------
// Description: The set of options used for connections. 
//----------------------------------------------------------------------------------------------------------------------
class Configuration::Options::Connection
{
public:
    static constexpr std::string_view Symbol = Symbols::Connection{};

    Connection();
    Connection(
        std::chrono::milliseconds const& timeout, std::int32_t limit, std::chrono::milliseconds const& interval);

    [[nodiscard]] bool operator==(Connection const& other) const noexcept;
    [[nodiscard]] std::strong_ordering operator<=>(Connection const& other) const noexcept;

    [[nodiscard]] static constexpr std::string_view GetFieldName() { return Symbol; }
    [[nodiscard]] static constexpr bool IsOptional() { return true; }

    [[nodiscard]] DeserializationResult Merge(Connection& other);
    [[nodiscard]] DeserializationResult Merge(
        boost::json::object const& json,
        std::string_view context = "",
        GlobalConnectionOptionsReference optGlobalOptions = {});
    [[nodiscard]] SerializationResult Write(
        boost::json::object& json, GlobalConnectionOptionsReference optGlobalOptions = {}) const;

    [[nodiscard]] ValidationResult AreOptionsAllowable() const;

    [[nodiscard]] std::chrono::milliseconds const& GetTimeout() const;
    [[nodiscard]] std::int32_t GetRetryLimit() const;
    [[nodiscard]] std::chrono::milliseconds const& GetRetryInterval() const;

    [[nodiscard]] bool SetTimeout(std::chrono::milliseconds const& value, bool& changed);
    [[nodiscard]] bool SetRetryLimit(std::int32_t value, bool& changed);
    [[nodiscard]] bool SetRetryInterval(std::chrono::milliseconds const& value, bool& changed);

private:
    OptionalConstructedField<Symbols::Timeout, std::chrono::milliseconds> m_optTimeout;
    Retry m_retryOptions;
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

    [[nodiscard]] bool operator==(Endpoint const& other) const noexcept;
    [[nodiscard]] std::strong_ordering operator<=>(Endpoint const& other) const noexcept;

    [[nodiscard]] DeserializationResult Merge(Endpoint& other);
    [[nodiscard]] DeserializationResult Merge(
        boost::json::object const& json,
        Runtime const& runtime,
        std::string_view context = "",
        GlobalConnectionOptionsReference optGlobalOptions = {});
    [[nodiscard]] SerializationResult Write(
        boost::json::object& json, GlobalConnectionOptionsReference optGlobalOptions = {}) const;

    [[nodiscard]] ValidationResult AreOptionsAllowable(std::string_view context = "") const;

    [[nodiscard]] ::Network::Protocol GetProtocol() const;
    [[nodiscard]] std::string const& GetProtocolString() const;
    [[nodiscard]] std::string const& GetInterface() const;
    [[nodiscard]] ::Network::BindingAddress const& GetBinding() const;
    [[nodiscard]] std::string const& GetBindingString() const;
    [[nodiscard]] std::optional<::Network::RemoteAddress> const& GetBootstrap() const;
    [[nodiscard]] bool UseBootstraps() const;
    [[nodiscard]] std::chrono::milliseconds const& GetConnectionTimeout() const;
    [[nodiscard]] std::int32_t GetConnectionRetryLimit() const;
    [[nodiscard]] std::chrono::milliseconds const& GetConnectionRetryInterval() const;

    void SetRuntimeOptions(Runtime const& runtime);
    [[nodiscard]] bool SetConnectionRetryLimit(std::int32_t limit, bool& changed);
    [[nodiscard]] bool SetConnectionTimeout(std::chrono::milliseconds const& timeout, bool& changed);
    [[nodiscard]] bool SetConnectionRetryInterval(std::chrono::milliseconds const& interval, bool& changed);

    UT_SupportMethod(static Endpoint CreateTestOptions(::Network::BindingAddress const& _binding));
    UT_SupportMethod(static Endpoint CreateTestOptions(std::string_view _interface, std::string_view _binding));

private:
    struct TransientValues {
        bool useBootstraps;
    };

    TransientValues m_transient;

    ConstructedField<Symbols::Protocol, ::Network::Protocol> m_protocol;
    Field<Symbols::Interface, std::string> m_interface;
    ConstructedField<Symbols::Binding, ::Network::BindingAddress> m_binding;
    OptionalConstructedField<Symbols::Bootstrap, ::Network::RemoteAddress> m_optBootstrap;
    Connection m_connectionOptions;
};

//----------------------------------------------------------------------------------------------------------------------
// Description: The set of options used to configure the network. 
//----------------------------------------------------------------------------------------------------------------------
class Configuration::Options::Network
{
public:
    static constexpr std::string_view Symbol = Symbols::Network{};

    using FetchedEndpoint = std::optional<std::reference_wrapper<Endpoint const>>;

    Network();

    [[nodiscard]] std::strong_ordering operator<=>(Network const& other) const noexcept;
    [[nodiscard]] bool operator==(Network const& other) const noexcept;

    [[nodiscard]] static constexpr std::string_view GetFieldName() { return Symbol; }
    [[nodiscard]] static constexpr bool IsOptional() { return false; }
 
    [[nodiscard]] DeserializationResult Merge(boost::json::object const& json, Runtime const& runtime);
    [[nodiscard]] SerializationResult Write(boost::json::object& json) const;

    [[nodiscard]] ValidationResult AreOptionsAllowable(Runtime const& runtime) const;

    [[nodiscard]] Endpoints const& GetEndpoints() const;
    [[nodiscard]] FetchedEndpoint GetEndpoint(::Network::BindingAddress const& binding) const;
    [[nodiscard]] FetchedEndpoint GetEndpoint(std::string_view const& uri) const;
    [[nodiscard]] FetchedEndpoint GetEndpoint(::Network::Protocol protocol, std::string_view const& binding) const;
    [[nodiscard]] std::chrono::milliseconds const& GetConnectionTimeout() const;
    [[nodiscard]] std::int32_t GetConnectionRetryLimit() const;
    [[nodiscard]] std::chrono::milliseconds const& GetConnectionRetryInterval() const;
    [[nodiscard]] std::optional<std::string> const& GetToken() const;

    [[nodiscard]] FetchedEndpoint UpsertEndpoint(Endpoint&& options, bool& changed);
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
    Connection m_connectionOptions;
    OptionalField<Symbols::Token, std::string> m_optToken;
};

//----------------------------------------------------------------------------------------------------------------------
// Description: The set of options used to configure a set of security algorithms
//----------------------------------------------------------------------------------------------------------------------
class Configuration::Options::Algorithms
{
public:
    explicit Algorithms(std::string_view fieldName);

    Algorithms(
        std::string_view fieldName,
        std::vector<std::string>&& keyAgreements,
        std::vector<std::string>&& ciphers,
        std::vector<std::string>&& hashFunctions);

    auto operator<=>(Algorithms const&) const;

    [[nodiscard]] std::string const& GetFieldName() const { return m_fieldName; }
    [[nodiscard]] bool Modified() const { return m_modified; }
    [[nodiscard]] bool NotModified() const { return !m_modified; }

    [[nodiscard]] DeserializationResult Merge(boost::json::object const& json, Runtime const& runtime);
    [[nodiscard]] SerializationResult Write(boost::json::object& json) const;

    [[nodiscard]] ValidationResult AreOptionsAllowable(Runtime const& runtime) const;
    [[nodiscard]] bool HasAtLeastOneAlgorithmForEachComponent() const;

    [[nodiscard]] std::vector<std::string> const& GetKeyAgreements() const { return m_keyAgreements; }
    [[nodiscard]] std::vector<std::string> const& GetCiphers() const { return m_ciphers; }
    [[nodiscard]] std::vector<std::string> const& GetHashFunctions() const { return m_hashFunctions; }

    [[nodiscard]] bool SetKeyAgreements(std::vector<std::string>&& keyAgreements, bool& changed);
    [[nodiscard]] bool SetCiphers(std::vector<std::string>&& ciphers, bool& changed);
    [[nodiscard]] bool SetHashFunctions(std::vector<std::string>&& hashFunctions, bool& changed);

private:
    std::string const m_fieldName;
    bool m_modified;

    std::vector<std::string> m_keyAgreements;
    std::vector<std::string> m_ciphers;
    std::vector<std::string> m_hashFunctions;
};

//----------------------------------------------------------------------------------------------------------------------
// Description: The set of options used to contain the supported algorithms by confidentiality level 
//----------------------------------------------------------------------------------------------------------------------
class Configuration::Options::SupportedAlgorithms
{
public:
    static constexpr std::string_view Symbol = Symbols::Algorithms{};
    
    using Container = std::map<
        ::Security::ConfidentialityLevel, Algorithms, std::greater<::Security::ConfidentialityLevel>>;

    using AlgorithmsReader = std::function<CallbackIteration(::Security::ConfidentialityLevel, Algorithms const&)>;
    using KeyAgreementReader = std::function<CallbackIteration(::Security::ConfidentialityLevel, std::string_view)>;
    using CipherReader = std::function<CallbackIteration(::Security::ConfidentialityLevel, std::string_view)>;
    using HashFunctionReader = std::function<CallbackIteration(::Security::ConfidentialityLevel, std::string_view)>;

    using FetchedAlgorithms = std::optional<std::reference_wrapper<Algorithms const>>;

    SupportedAlgorithms();
    // TODO: Add flag for validating supplied algorithms 
    explicit SupportedAlgorithms(Container&& container);

    [[nodiscard]] static constexpr std::string_view GetFieldName() { return Symbol; }
    [[nodiscard]] static constexpr bool IsOptional() { return false; }

    [[nodiscard]] bool Modified() const { return m_modified; }
    [[nodiscard]] bool NotModified() const { return !m_modified; }

    [[nodiscard]] DeserializationResult Merge(boost::json::object const& json, Runtime const& runtime);
    [[nodiscard]] SerializationResult Write(boost::json::object& json) const;

    [[nodiscard]] bool operator==(SupportedAlgorithms const& other) const noexcept;
    [[nodiscard]] std::strong_ordering operator<=>(SupportedAlgorithms const& other) const noexcept;

    [[nodiscard]] ValidationResult AreOptionsAllowable(Runtime const& runtime) const;
    [[nodiscard]] bool HasAlgorithmsForLevel(::Security::ConfidentialityLevel level) const;
    [[nodiscard]] std::size_t Size() const { return m_container.size(); }
    [[nodiscard]] bool Empty() const { return m_container.empty(); }

    [[nodiscard]] FetchedAlgorithms FetchAlgorithms(::Security::ConfidentialityLevel level) const;

    bool ForEachSupportedAlgorithm(AlgorithmsReader const& reader) const;
    bool ForEachSupportedKeyAgreement(KeyAgreementReader const& reader) const;
    bool ForEachSupportedCipher(CipherReader& reader) const;
    bool ForEachSupportedHashFunction(HashFunctionReader& reader) const;

    [[nodiscard]] bool SetAlgorithmsAtLevel(
        ::Security::ConfidentialityLevel level,
        std::vector<std::string> keyAgreements,
        std::vector<std::string> ciphers,
        std::vector<std::string> hashFunctions,
        bool& changed);

    void Clear() { m_modified = true; m_container.clear(); }

private:
    bool m_modified;
    Container m_container;
};

//----------------------------------------------------------------------------------------------------------------------
// Description: The set of options used to configure the security strategy. 
//----------------------------------------------------------------------------------------------------------------------
class Configuration::Options::Security
{
public:
    static constexpr std::string_view Symbol = Symbols::Security{};

    Security() = default;
    explicit Security(SupportedAlgorithms&& supportedAlgorithms) : m_supportedAlgorithms(std::move(supportedAlgorithms)) {}

    [[nodiscard]] bool operator==(Security const& other) const noexcept;
    [[nodiscard]] std::strong_ordering operator<=>(Security const& other) const noexcept;

    [[nodiscard]] static constexpr std::string_view GetFieldName() { return Symbol; }
    [[nodiscard]] static constexpr bool IsOptional() { return false; }
 
    [[nodiscard]] DeserializationResult Merge(boost::json::object const& json, Runtime const& runtime);
    [[nodiscard]] SerializationResult Write(boost::json::object& json) const;

    [[nodiscard]] ValidationResult AreOptionsAllowable(Runtime const& runtime) const;

    [[nodiscard]] SupportedAlgorithms const& GetSupportedAlgorithms() const;

    //[[nodiscard]] bool SetCipherSuite(SuiteIdentifier) const; // TODO: ...?

    [[nodiscard]] bool SetSupportedAlgorithmsAtLevel(
        ::Security::ConfidentialityLevel level,
        std::vector<std::string> keyAgreements,
        std::vector<std::string> ciphers,
        std::vector<std::string> hashFunctions,
        bool& changed);

    void ClearSupportedAlgorithms();

private:
    SupportedAlgorithms m_supportedAlgorithms;
};

//----------------------------------------------------------------------------------------------------------------------
