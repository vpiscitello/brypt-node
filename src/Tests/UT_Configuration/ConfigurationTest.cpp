//----------------------------------------------------------------------------------------------------------------------
#include "Components/Configuration/Defaults.hpp"
#include "Components/Configuration/Parser.hpp"
#include "Components/Configuration/Parser.hpp"
#include "Components/Security/SecurityDefinitions.hpp"
#include "Utilities/NodeUtils.hpp"
//----------------------------------------------------------------------------------------------------------------------
#include <gtest/gtest.h>
//----------------------------------------------------------------------------------------------------------------------
#include <cstdint>
#include <chrono>
#include <string>
#include <string_view>
//----------------------------------------------------------------------------------------------------------------------

//----------------------------------------------------------------------------------------------------------------------
namespace {
namespace local {
//----------------------------------------------------------------------------------------------------------------------

std::filesystem::path GetFilepath(std::filesystem::path const& filename);

//----------------------------------------------------------------------------------------------------------------------
} // local namespace
//----------------------------------------------------------------------------------------------------------------------
namespace test {
//----------------------------------------------------------------------------------------------------------------------

constexpr auto RuntimeOptions = Configuration::Options::Runtime
{ 
    .context = RuntimeContext::Foreground,
    .verbosity = spdlog::level::debug,
    .useInteractiveConsole = false,
    .useBootstraps = false,
    .useFilepathDeduction = false
};

Network::BindingAddress const BindingAddress{ Network::Protocol::TCP, "*:35216", "lo" };

//----------------------------------------------------------------------------------------------------------------------
} // local namespace
} // namespace
//----------------------------------------------------------------------------------------------------------------------

TEST(ConfigurationParserSuite, GenerateConfigurationFilepathTest)
{
    auto const filepath = Configuration::GetDefaultConfigurationFilepath();
    EXPECT_TRUE(filepath.has_parent_path());
    EXPECT_TRUE(filepath.is_absolute());
    auto const found = filepath.string().find(Configuration::DefaultBryptFolder);
    EXPECT_NE(found, std::string::npos);
    EXPECT_EQ(filepath.filename(), Configuration::DefaultConfigurationFilename);
}

//----------------------------------------------------------------------------------------------------------------------

TEST(ConfigurationParserSuite, ParseGoodFileTest)
{
    Configuration::Parser parser(local::GetFilepath("good/config.json"), test::RuntimeOptions);
    EXPECT_FALSE(parser.FilesystemDisabled());
    EXPECT_FALSE(parser.Validated());
    EXPECT_FALSE(parser.Changed());
    EXPECT_EQ(parser.FetchOptions(), Configuration::StatusCode::Success);
    EXPECT_TRUE(parser.Validated());
    EXPECT_FALSE(parser.Changed());
}

//----------------------------------------------------------------------------------------------------------------------

TEST(ConfigurationParserSuite, ParseMalformedFileTest)
{
    Configuration::Parser parser(local::GetFilepath("malformed/config.json"), test::RuntimeOptions);
    EXPECT_FALSE(parser.FilesystemDisabled());
    EXPECT_FALSE(parser.Validated());
    EXPECT_FALSE(parser.Changed());
    EXPECT_NE(parser.FetchOptions(), Configuration::StatusCode::Success);
    EXPECT_FALSE(parser.Validated());
    EXPECT_FALSE(parser.Changed());
}

//----------------------------------------------------------------------------------------------------------------------

TEST(ConfigurationParserSuite, ParseMissingFileTest)
{
    Configuration::Parser parser(local::GetFilepath("missing/config.json"), test::RuntimeOptions);
    EXPECT_FALSE(parser.FilesystemDisabled());
    EXPECT_FALSE(parser.Validated());
    EXPECT_FALSE(parser.Changed());
    EXPECT_EQ(parser.FetchOptions(), Configuration::StatusCode::FileError);
    EXPECT_FALSE(parser.Validated());
    EXPECT_FALSE(parser.Changed());
}

//----------------------------------------------------------------------------------------------------------------------

TEST(ConfigurationParserSuite, FileGenerationTest)
{
    using namespace std::chrono_literals;
    Configuration::Parser parser(local::GetFilepath("good/generated.json"), test::RuntimeOptions);
    if (std::filesystem::exists(parser.GetFilepath())) { std::filesystem::remove(parser.GetFilepath()); }

    EXPECT_FALSE(parser.FilesystemDisabled());
    EXPECT_FALSE(parser.Validated());
    EXPECT_FALSE(parser.Changed());
    EXPECT_EQ(parser.FetchOptions(), Configuration::StatusCode::FileError);

    // Verify the default options are set the the expected values. 
    EXPECT_EQ(parser.GetRuntimeContext(), test::RuntimeOptions.context);
    EXPECT_EQ(parser.GetVerbosity(), test::RuntimeOptions.verbosity);
    EXPECT_EQ(parser.UseInteractiveConsole(), test::RuntimeOptions.useInteractiveConsole);
    EXPECT_EQ(parser.UseBootstraps(), test::RuntimeOptions.useBootstraps);
    EXPECT_EQ(parser.UseFilepathDeduction(), test::RuntimeOptions.useFilepathDeduction);
    EXPECT_EQ(parser.GetIdentifierType(), Configuration::Options::Identifier::Type::Invalid);
    EXPECT_FALSE(parser.GetNodeIdentifier());
    EXPECT_EQ(parser.GetNodeName(), "");
    EXPECT_EQ(parser.GetNodeDescription(), "");
    EXPECT_EQ(parser.GetNodeLocation(), "");
    EXPECT_TRUE(parser.GetEndpoints().empty());
    EXPECT_EQ(parser.GetConnectionTimeout(), Configuration::Defaults::ConnectionTimeout);
    EXPECT_EQ(parser.GetConnectionRetryLimit(), Configuration::Defaults::ConnectionRetryLimit);
    EXPECT_EQ(parser.GetConnectionRetryInterval(), Configuration::Defaults::ConnectionRetryInterval);
    EXPECT_EQ(parser.GetSecurityStrategy(), Security::Strategy::Invalid);
    EXPECT_FALSE(parser.GetNetworkToken());

    // Verify that that all setters and getters work as expected. 
    parser.SetRuntimeContext(RuntimeContext::Background);
    EXPECT_EQ(parser.GetRuntimeContext(), RuntimeContext::Background);

    parser.SetVerbosity(spdlog::level::info);
    EXPECT_EQ(parser.GetVerbosity(), spdlog::level::info);

    parser.SetUseInteractiveConsole(true);
    EXPECT_TRUE(parser.UseInteractiveConsole());

    parser.SetUseBootstraps(true);
    EXPECT_TRUE(parser.UseBootstraps());

    parser.SetUseFilepathDeduction(true);
    EXPECT_TRUE(parser.UseFilepathDeduction());

    EXPECT_TRUE(parser.SetNodeIdentifier(Configuration::Options::Identifier::Type::Persistent));
    EXPECT_EQ(parser.GetIdentifierType(), Configuration::Options::Identifier::Type::Persistent);
    EXPECT_TRUE(parser.GetNodeIdentifier());

    EXPECT_TRUE(parser.SetNodeName("node_name"));
    EXPECT_EQ(parser.GetNodeName(), "node_name");

    EXPECT_TRUE(parser.SetNodeDescription("node_description"));
    EXPECT_EQ(parser.GetNodeDescription(), "node_description");

    EXPECT_TRUE(parser.SetNodeLocation("node_location"));
    EXPECT_EQ(parser.GetNodeLocation(), "node_location");

    EXPECT_TRUE(parser.UpsertEndpoint({ "TCP", "lo", "*:35216", "127.0.0.1:35217" }));
    EXPECT_TRUE(parser.GetEndpoint(test::BindingAddress.GetUri()));

    EXPECT_FALSE(parser.SetConnectionTimeout(1441min));
    EXPECT_TRUE(parser.SetConnectionTimeout(0s));
    EXPECT_TRUE(parser.SetConnectionTimeout(1440min));
    EXPECT_TRUE(parser.SetConnectionTimeout(250ms));
    EXPECT_EQ(parser.GetConnectionTimeout(), 250ms);

    EXPECT_FALSE(parser.SetConnectionRetryLimit(-1));
    EXPECT_TRUE(parser.SetConnectionRetryLimit(0));
    EXPECT_TRUE(parser.SetConnectionRetryLimit(15));
    EXPECT_EQ(parser.GetConnectionRetryLimit(), 15);

    EXPECT_FALSE(parser.SetConnectionRetryInterval(1441min));
    EXPECT_TRUE(parser.SetConnectionRetryInterval(0s));
    EXPECT_TRUE(parser.SetConnectionRetryInterval(1440min));
    EXPECT_TRUE(parser.SetConnectionRetryInterval(250ms));
    EXPECT_EQ(parser.GetConnectionRetryInterval(), 250ms);

    parser.SetSecurityStrategy(Security::Strategy::PQNISTL3);
    EXPECT_EQ(parser.GetSecurityStrategy(), Security::Strategy::PQNISTL3);

    EXPECT_TRUE(parser.SetNetworkToken("network_token"));
    ASSERT_TRUE(parser.GetNetworkToken());
    EXPECT_EQ(*parser.GetNetworkToken(), "network_token");

    EXPECT_FALSE(parser.Validated());
    EXPECT_TRUE(parser.Changed());

    EXPECT_EQ(parser.Serialize(), Configuration::StatusCode::Success);
    EXPECT_TRUE(parser.Validated());
    EXPECT_FALSE(parser.Changed());

    Configuration::Parser checker(local::GetFilepath("good/generated.json"), test::RuntimeOptions);
    EXPECT_EQ(checker.FetchOptions(), Configuration::StatusCode::Success);
    EXPECT_TRUE(checker.Validated());
    EXPECT_FALSE(checker.Changed());

    // Verify the fields that are not written to the file are not changed after a read. 
    EXPECT_EQ(checker.GetRuntimeContext(), test::RuntimeOptions.context);
    EXPECT_EQ(checker.GetVerbosity(), test::RuntimeOptions.verbosity);
    EXPECT_EQ(checker.UseInteractiveConsole(), test::RuntimeOptions.useInteractiveConsole);
    EXPECT_EQ(checker.UseBootstraps(), test::RuntimeOptions.useBootstraps);
    EXPECT_EQ(checker.UseFilepathDeduction(), test::RuntimeOptions.useFilepathDeduction);

    // Verify the check and original parser values match. 
    EXPECT_EQ(checker.GetIdentifierType(), parser.GetIdentifierType());
    EXPECT_EQ(checker.GetNodeName(), parser.GetNodeName());
    EXPECT_EQ(checker.GetNodeDescription(), parser.GetNodeDescription());
    EXPECT_EQ(checker.GetNodeLocation(), parser.GetNodeLocation());
    EXPECT_EQ(checker.GetEndpoints().size(), parser.GetEndpoints().size());
    EXPECT_EQ(checker.GetSecurityStrategy(), parser.GetSecurityStrategy());
    EXPECT_EQ(checker.GetConnectionTimeout(), parser.GetConnectionTimeout());
    EXPECT_EQ(checker.GetConnectionRetryLimit(), parser.GetConnectionRetryLimit());
    EXPECT_EQ(checker.GetConnectionRetryInterval(), parser.GetConnectionRetryInterval());
    EXPECT_EQ(checker.GetNetworkToken(), parser.GetNetworkToken());

    {
        auto const spIdentifier = parser.GetNodeIdentifier();
        auto const spCheckIdentifier = checker.GetNodeIdentifier();
        ASSERT_TRUE(spCheckIdentifier && spIdentifier);
        EXPECT_EQ(*spCheckIdentifier, *spIdentifier);
    }

    {
        auto const optEndpoint = parser.GetEndpoint(test::BindingAddress.GetUri());
        auto const optCheckEndpoint = checker.GetEndpoint(test::BindingAddress.GetUri());
        ASSERT_TRUE(optCheckEndpoint && optEndpoint);
        EXPECT_EQ(optCheckEndpoint->get(), optEndpoint->get());
        EXPECT_EQ(optEndpoint->get().UseBootstraps(), parser.UseBootstraps());
    }

    std::filesystem::remove(parser.GetFilepath());
    EXPECT_FALSE(std::filesystem::exists(parser.GetFilepath())); // Verify the file has been successfully deleted
}

//----------------------------------------------------------------------------------------------------------------------

TEST(ConfigurationParserSuite, MergeOptionsTest)
{
    using namespace std::chrono_literals;
    Configuration::Parser parser(local::GetFilepath("good/generated.json"), test::RuntimeOptions);
    if (std::filesystem::exists(parser.GetFilepath())) { std::filesystem::remove(parser.GetFilepath()); }

    EXPECT_TRUE(parser.SetNodeIdentifier(Configuration::Options::Identifier::Type::Persistent));
    EXPECT_TRUE(parser.SetNodeName("original_name"));
    EXPECT_TRUE(parser.SetNodeDescription("original_description"));
    EXPECT_TRUE(parser.SetNodeLocation("original_location"));
    EXPECT_TRUE(parser.UpsertEndpoint({ "TCP", "original_interface", "127.0.0.1:35216" }));
    EXPECT_TRUE(parser.SetConnectionTimeout(250ms));
    EXPECT_TRUE(parser.SetConnectionRetryLimit(15));
    EXPECT_TRUE(parser.SetConnectionRetryInterval(250ms));

    parser.SetSecurityStrategy(Security::Strategy::PQNISTL3);
    EXPECT_TRUE(parser.SetNetworkToken("original_token"));

    EXPECT_EQ(parser.Serialize(), Configuration::StatusCode::Success);
    EXPECT_TRUE(parser.Validated());
    EXPECT_FALSE(parser.Changed());

    Configuration::Parser merger(local::GetFilepath("good/generated.json"), test::RuntimeOptions);

    // Set some values before deserializing the configuration file. 
    EXPECT_TRUE(merger.SetNodeIdentifier(Configuration::Options::Identifier::Type::Ephemeral));
    EXPECT_TRUE(merger.SetNodeLocation("merge_location"));
    EXPECT_TRUE(merger.UpsertEndpoint({ "TCP", "merge_interface", "127.0.0.1:35216", "127.0.0.1:35217" }));
    EXPECT_TRUE(merger.UpsertEndpoint({ "TCP", "merge_interface", "127.0.0.1:35226" }));
    EXPECT_TRUE(merger.SetNetworkToken("merge_token"));
    EXPECT_TRUE(merger.SetConnectionTimeout(500ms));
    EXPECT_TRUE(merger.SetConnectionRetryLimit(30));

    EXPECT_EQ(merger.FetchOptions(), Configuration::StatusCode::Success);
    EXPECT_TRUE(merger.Validated());
    EXPECT_FALSE(merger.Changed());

    // Verify the merged values have been chosen correctly. Values that were set before, reading the file should
    // be selected over the values from the file. 
    EXPECT_EQ(merger.GetIdentifierType(), parser.GetIdentifierType()); // The persistent identifier should be chosen.
    EXPECT_EQ(merger.GetNodeName(), parser.GetNodeName());
    EXPECT_EQ(merger.GetNodeDescription(), parser.GetNodeDescription());
    EXPECT_NE(merger.GetNodeLocation(), parser.GetNodeLocation()); // The node location should differ. 
    EXPECT_EQ(merger.GetNodeLocation(), "merge_location");
    EXPECT_NE(merger.GetEndpoints().size(), parser.GetEndpoints().size()); // The endpoints should differ.
    EXPECT_EQ(merger.GetConnectionTimeout(), 500ms);
    EXPECT_EQ(merger.GetConnectionRetryLimit(), 30);
    EXPECT_EQ(merger.GetConnectionRetryInterval(), parser.GetConnectionRetryInterval());
    EXPECT_EQ(merger.GetSecurityStrategy(), parser.GetSecurityStrategy());
    ASSERT_TRUE(merger.GetNetworkToken());
    EXPECT_NE(merger.GetNetworkToken(), parser.GetNetworkToken()); // The network token should differ. 
    EXPECT_EQ(*merger.GetNetworkToken(), "merge_token"); // The network token should differ. 

    {
        auto const spIdentifier = parser.GetNodeIdentifier();
        auto const spCheckIdentifier = merger.GetNodeIdentifier();
        ASSERT_TRUE(spCheckIdentifier && spIdentifier);
        EXPECT_EQ(*spCheckIdentifier, *spIdentifier); // The identifier value should differ. 
    }

    {
        auto const optEndpoint = parser.GetEndpoint("tcp://127.0.0.1:35216");
        auto const optCheckEndpoint = merger.GetEndpoint("tcp://127.0.0.1:35216");
        ASSERT_TRUE(optCheckEndpoint && optEndpoint);

        EXPECT_NE(optCheckEndpoint->get(), optEndpoint->get()); // This endpoint should have the updates. 

        EXPECT_EQ(optEndpoint->get().GetInterface(), "original_interface");
        EXPECT_EQ(optCheckEndpoint->get().GetInterface(), "merge_interface");

        EXPECT_FALSE(optEndpoint->get().GetBootstrap());
        ASSERT_TRUE(optCheckEndpoint->get().GetBootstrap());
        EXPECT_EQ(optCheckEndpoint->get().GetBootstrap()->GetUri(), "tcp://127.0.0.1:35217");
        EXPECT_EQ(optEndpoint->get().UseBootstraps(), parser.UseBootstraps());

        EXPECT_TRUE(merger.GetEndpoint("tcp://127.0.0.1:35226")); // This endpoint should have been added.
    }

    std::filesystem::remove(parser.GetFilepath());
    EXPECT_FALSE(std::filesystem::exists(parser.GetFilepath())); // Verify the file has been successfully deleted
}

//----------------------------------------------------------------------------------------------------------------------

TEST(ConfigurationParserSuite, DisableFilesystemTest)
{
    Configuration::Parser parser(test::RuntimeOptions);
    EXPECT_TRUE(parser.FilesystemDisabled());
    EXPECT_FALSE(parser.Validated());
    EXPECT_FALSE(parser.Changed());

    // All fields should be defaulted to empty, invalid, or resonable defaults. 
    EXPECT_EQ(parser.GetIdentifierType(), Configuration::Options::Identifier::Type::Invalid);
    EXPECT_FALSE(parser.GetNodeIdentifier());
    EXPECT_EQ(parser.GetNodeName(), "");
    EXPECT_EQ(parser.GetNodeDescription(), "");
    EXPECT_EQ(parser.GetNodeLocation(), "");
    EXPECT_TRUE(parser.GetEndpoints().empty());
    EXPECT_EQ(parser.GetConnectionTimeout(), Configuration::Defaults::ConnectionTimeout);
    EXPECT_EQ(parser.GetConnectionRetryLimit(), Configuration::Defaults::ConnectionRetryLimit);
    EXPECT_EQ(parser.GetConnectionRetryInterval(), Configuration::Defaults::ConnectionRetryInterval);
    EXPECT_EQ(parser.GetSecurityStrategy(), Security::Strategy::Invalid);
    EXPECT_FALSE(parser.GetNetworkToken());

    EXPECT_EQ(parser.FetchOptions(), Configuration::StatusCode::DecodeError);
    EXPECT_FALSE(parser.Validated());
    EXPECT_FALSE(parser.Changed());
    
    // No field should change after a failed fetch if no values have changed. 
    EXPECT_EQ(parser.GetIdentifierType(), Configuration::Options::Identifier::Type::Invalid);
    EXPECT_FALSE(parser.GetNodeIdentifier());
    EXPECT_EQ(parser.GetNodeName(), "");
    EXPECT_EQ(parser.GetNodeDescription(), "");
    EXPECT_EQ(parser.GetNodeLocation(), "");
    EXPECT_TRUE(parser.GetEndpoints().empty());
    EXPECT_EQ(parser.GetConnectionTimeout(), Configuration::Defaults::ConnectionTimeout);
    EXPECT_EQ(parser.GetConnectionRetryLimit(), Configuration::Defaults::ConnectionRetryLimit);
    EXPECT_EQ(parser.GetConnectionRetryInterval(), Configuration::Defaults::ConnectionRetryInterval);
    EXPECT_EQ(parser.GetSecurityStrategy(), Security::Strategy::Invalid);
    EXPECT_FALSE(parser.GetNetworkToken());

    // The parser should flip the changed flag when a field has been set. 
    EXPECT_TRUE(parser.SetNodeIdentifier(Configuration::Options::Identifier::Type::Ephemeral));
    EXPECT_FALSE(parser.Validated());
    EXPECT_TRUE(parser.Changed());

    // The identifier field should be initialized after setting the type. 
    EXPECT_EQ(parser.GetIdentifierType(), Configuration::Options::Identifier::Type::Ephemeral);
    auto const spIdentifier = parser.GetNodeIdentifier();
    ASSERT_TRUE(spIdentifier);
    EXPECT_TRUE(spIdentifier->IsValid());

    std::string const identifier = *spIdentifier; // Sanity check the external representation. 
    EXPECT_GE(identifier.size(), Node::Identifier::MinimumSize);
    EXPECT_LE(identifier.size(), Node::Identifier::MaximumSize);

    // The parser should still fail to fetch if not all of the required fields are set. 
    EXPECT_EQ(parser.FetchOptions(), Configuration::StatusCode::DecodeError);
    EXPECT_FALSE(parser.Validated());
    EXPECT_TRUE(parser.Changed());

    // The identifier should remain unchanged after fetching. 
    EXPECT_EQ(parser.GetIdentifierType(), Configuration::Options::Identifier::Type::Ephemeral);
    ASSERT_TRUE(parser.GetNodeIdentifier());
    EXPECT_EQ(*parser.GetNodeIdentifier(), *spIdentifier);

    parser.SetSecurityStrategy(Security::Strategy::PQNISTL3); // You should be able to set a valid security strategy. 
    EXPECT_EQ(parser.GetSecurityStrategy(), Security::Strategy::PQNISTL3);

    // You should be able to insert a new valid endpoint configuration.
    EXPECT_TRUE(parser.UpsertEndpoint({ "TCP", "lo", "*:35216", "127.0.0.1:35217" }));
    EXPECT_EQ(parser.GetEndpoints().size(), 1);

    // You should be able to fetch an initalized endpoint configuration after storing one.
    {
        Network::RemoteAddress const bootstrap = { 
            Network::Protocol::TCP, "127.0.0.1:35217", true, Network::RemoteAddress::Origin::Cache };

        auto const optEndpoint = parser.GetEndpoint(test::BindingAddress);
        ASSERT_TRUE(optEndpoint);
        EXPECT_EQ(optEndpoint->get().GetProtocol(), Network::Protocol::TCP);
        EXPECT_EQ(optEndpoint->get().GetProtocolString(), "TCP");
        EXPECT_EQ(optEndpoint->get().GetInterface(), "lo");
        EXPECT_EQ(optEndpoint->get().GetBinding(), test::BindingAddress);

        auto optStoreBootstrap = optEndpoint->get().GetBootstrap();
        ASSERT_TRUE(optStoreBootstrap);
        EXPECT_EQ(*optStoreBootstrap, bootstrap);
        EXPECT_EQ(optEndpoint->get().UseBootstraps(), parser.UseBootstraps());

        auto const optUriEndpoint = parser.GetEndpoint(test::BindingAddress.GetUri()); 
        ASSERT_TRUE(optUriEndpoint);
        EXPECT_EQ(optEndpoint->get(), optUriEndpoint->get());

        auto const optProtocolEndpoint = parser.GetEndpoint(test::BindingAddress.GetProtocol(), "*:35216");
        ASSERT_TRUE(optProtocolEndpoint);
        EXPECT_EQ(optEndpoint->get(), optProtocolEndpoint->get());

        auto const& endpoints = parser.GetEndpoints();
        ASSERT_EQ(endpoints.size(), 1);
        EXPECT_EQ(optEndpoint->get(), endpoints.front());
    }

    // You should be able to update an existing endpoint configuration.
    EXPECT_TRUE(parser.UpsertEndpoint({ "TCP", "lo", "*:35216", "127.0.0.1:35218" }));
    EXPECT_EQ(parser.GetEndpoints().size(), 1);

    // You should be able to fetch the updated endpoint.
    {
        Network::RemoteAddress const bootstrap = { 
            Network::Protocol::TCP, "127.0.0.1:35218", true, Network::RemoteAddress::Origin::Cache };

        auto const optEndpoint = parser.GetEndpoint(test::BindingAddress.GetUri());
        ASSERT_TRUE(optEndpoint);
        
        auto const optStoreBootstrap = optEndpoint->get().GetBootstrap();
        ASSERT_TRUE(optStoreBootstrap);
        EXPECT_EQ(*optStoreBootstrap, bootstrap);
        EXPECT_EQ(optEndpoint->get().UseBootstraps(), parser.UseBootstraps());
    }

    // You should not be able to fetch a missing endpoint. 
    EXPECT_FALSE(parser.GetEndpoint("tcp://127.0.0.1:35217"));

    // You should not be able to set an invalid endpoint. 
    EXPECT_FALSE(parser.UpsertEndpoint({ "Invalid", "lo", "*:35216", "127.0.0.1:35218" }));
    EXPECT_FALSE(parser.UpsertEndpoint({ "Invalid", "lo", "abcd", "127.0.0.1:35218" }));
    EXPECT_FALSE(parser.UpsertEndpoint({ "Invalid", "lo", "*:35216", "abcd" }));

    // The validation and changed flags should remain the same after applying all updates. 
    EXPECT_FALSE(parser.Validated());
    EXPECT_TRUE(parser.Changed());

    // You should be able to fetch the options with all required fields set, this should validate the changes. 
    EXPECT_EQ(parser.FetchOptions(), Configuration::StatusCode::Success);
    EXPECT_TRUE(parser.Validated());
    EXPECT_FALSE(parser.Changed());

    // You should be able to remove an existing endpoint
    {
        EXPECT_TRUE(parser.ExtractEndpoint(test::BindingAddress));
        EXPECT_FALSE(parser.GetEndpoint(test::BindingAddress));
        EXPECT_FALSE(parser.Validated());
        EXPECT_TRUE(parser.Changed());
        
        EXPECT_FALSE(parser.ExtractEndpoint(test::BindingAddress)); // You should not be able to remove it twice.

        // You should be able to re-add it.
        EXPECT_TRUE(parser.UpsertEndpoint({ "TCP", "lo", "*:35216", "127.0.0.1:35218" }));
        EXPECT_TRUE(parser.GetEndpoint(test::BindingAddress));
        EXPECT_FALSE(parser.Validated());
        EXPECT_TRUE(parser.Changed());

        EXPECT_TRUE(parser.ExtractEndpoint(test::BindingAddress.GetUri()));

        EXPECT_TRUE(parser.UpsertEndpoint({ "TCP", "lo", "*:35216", "127.0.0.1:35218" }));
        EXPECT_TRUE(parser.ExtractEndpoint(Network::Protocol::TCP, "*:35216"));
    }

    // You should not be able fetch the options after removing a required component. 
    EXPECT_EQ(parser.FetchOptions(), Configuration::StatusCode::DecodeError);
    EXPECT_FALSE(parser.Validated());
    EXPECT_TRUE(parser.Changed());
}

//----------------------------------------------------------------------------------------------------------------------

std::filesystem::path local::GetFilepath(std::filesystem::path const& filename)
{
    auto path = std::filesystem::current_path();
    if (path.filename() == "UT_Configuration") { 
        return path / "files" / filename;
    } else {
#if defined(WIN32)
        path = path.parent_path();
#endif
        if (path.filename() == "bin") { path.remove_filename(); }
        return path / "Tests/UT_Configuration/files" / filename;
    }
}

//----------------------------------------------------------------------------------------------------------------------
