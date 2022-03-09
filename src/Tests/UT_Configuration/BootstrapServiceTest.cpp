//----------------------------------------------------------------------------------------------------------------------
#include "BryptIdentifier/BryptIdentifier.hpp"
#include "BryptIdentifier/ReservedIdentifiers.hpp"
#include "Components/Configuration/Options.hpp"
#include "Components/Configuration/BootstrapService.hpp"
#include "Components/Network/Protocol.hpp"
#include "Components/Network/Address.hpp"
#include "Components/Peer/Proxy.hpp"
#include "Components/Scheduler/Registrar.hpp"
#include "Utilities/NodeUtils.hpp"
#include "Utilities/InvokeContext.hpp"
#include "Utilities/Logger.hpp"
//----------------------------------------------------------------------------------------------------------------------
#include <gtest/gtest.h>
//----------------------------------------------------------------------------------------------------------------------
#include <cstdint>
#include <chrono>
#include <filesystem>
#include <random>
#include <ranges>
#include <string>
#include <string_view>
//----------------------------------------------------------------------------------------------------------------------

//----------------------------------------------------------------------------------------------------------------------
namespace {
namespace local {
//----------------------------------------------------------------------------------------------------------------------

std::filesystem::path GetFilepath(std::filesystem::path const& filename);
Configuration::Options::Endpoint GenerateTcpOptions(std::uint16_t port);

//----------------------------------------------------------------------------------------------------------------------
} // local namespace
//----------------------------------------------------------------------------------------------------------------------
namespace test {
//----------------------------------------------------------------------------------------------------------------------

constexpr Network::Endpoint::Identifier EndpointIdentifier = 1;
constexpr std::string_view TcpBootstrapBase = "tcp://127.0.0.1:";
constexpr std::uint16_t TcpBasePort = 35216;
constexpr std::size_t FileStoredCount = 4;

constexpr auto BootstrapGenerator = [] (std::size_t port) mutable -> Network::RemoteAddress
    { return { Network::Protocol::TCP, test::TcpBootstrapBase.data() + std::to_string(port), true }; };

//----------------------------------------------------------------------------------------------------------------------
} // local namespace
} // namespace
//----------------------------------------------------------------------------------------------------------------------

// TODO: Disable disabled file usage

TEST(BootstrapServiceSuite, GenerateBootstrapFilepathTest)
{
    auto const filepath = Configuration::GetDefaultBootstrapFilepath();
    EXPECT_TRUE(filepath.has_parent_path());
    EXPECT_TRUE(filepath.is_absolute());
    EXPECT_EQ(filepath.filename(), Configuration::DefaultBootstrapFilename);
    EXPECT_NE(filepath.string().find(Configuration::DefaultBryptFolder), std::string::npos);
}

//----------------------------------------------------------------------------------------------------------------------

TEST(BootstrapServiceSuite, DefualtBootstrapTest)
{
    std::filesystem::path const filepath = local::GetFilepath("good/defaults.json");
    EXPECT_FALSE(std::filesystem::exists(filepath)); // This test is expected to delete the generated file.

    Configuration::Options::Endpoints configuration;
    configuration.emplace_back(local::GenerateTcpOptions(test::TcpBasePort));
    auto const expected = configuration.front().GetBootstrap()->GetUri();

    // Verify we can initalize the cache state and generate the file from the provided configuration.
    {
        auto const spRegistrar = std::make_shared<Scheduler::Registrar>();
        BootstrapService service(filepath);
        service.SetDefaults(configuration);
        service.Register(spRegistrar);

        EXPECT_TRUE(service.FetchBootstraps());
        EXPECT_EQ(service.BootstrapCount(), std::size_t(1));
        EXPECT_EQ(service.BootstrapCount(Network::Protocol::TCP), std::size_t(1));
        std::size_t const read = service.ForEachBootstrap(Network::Protocol::TCP,
            [&expected] (Network::RemoteAddress const& bootstrap) -> CallbackIteration
            {
                // With defaults set, we expect the origin of the bootstrap to be from the user. 
                EXPECT_EQ(bootstrap.GetOrigin(), Network::RemoteAddress::Origin::User);
                EXPECT_EQ(bootstrap.GetUri(), expected);
                return CallbackIteration::Continue;
            });
        EXPECT_EQ(read, service.BootstrapCount());
    }

    EXPECT_TRUE(std::filesystem::exists(filepath)); // On destruction the service should write to the file.

    // Verify we can read the file generated from the defaults. 
    {
        auto const spRegistrar = std::make_shared<Scheduler::Registrar>();
        BootstrapService service(filepath);
        service.Register(spRegistrar);

        EXPECT_TRUE(service.FetchBootstraps());
        EXPECT_EQ(service.BootstrapCount(), std::size_t(1));
        EXPECT_EQ(service.BootstrapCount(Network::Protocol::TCP), std::size_t(1));
        std::size_t const read = service.ForEachBootstrap(Network::Protocol::TCP,
            [&expected] (Network::RemoteAddress const& bootstrap) -> CallbackIteration
            {
                // Without defaults set, we expect the origin of the bootstrap to be from the cache. 
                EXPECT_EQ(bootstrap.GetOrigin(), Network::RemoteAddress::Origin::Cache);
                EXPECT_EQ(bootstrap.GetUri(), expected);
                return CallbackIteration::Continue;
            });
        EXPECT_EQ(read, service.BootstrapCount());
    }

    std::filesystem::remove(filepath);
    EXPECT_FALSE(std::filesystem::exists(filepath)); // Verify the file has been successfully deleted
}

//----------------------------------------------------------------------------------------------------------------------

TEST(BootstrapServiceSuite, ParseGoodFileTest)
{
    std::array<Network::RemoteAddress, test::FileStoredCount> expectations;
    std::ranges::generate(expectations.begin(), expectations.end(), 
        [current = std::uint16_t(test::TcpBasePort)] () mutable { return test::BootstrapGenerator(current++); });

    auto const spRegistrar = std::make_shared<Scheduler::Registrar>();
    BootstrapService service(local::GetFilepath("good/bootstrap.json"));
    service.Register(spRegistrar);

    ASSERT_TRUE(service.FetchBootstraps()); // Verify we can read a non default generated file. 
    EXPECT_EQ(service.BootstrapCount(), test::FileStoredCount);
    EXPECT_EQ(service.BootstrapCount(Network::Protocol::TCP), test::FileStoredCount);
    std::size_t const read = service.ForEachBootstrap(Network::Protocol::TCP,
        [&expectations] (Network::RemoteAddress const& bootstrap) mutable -> CallbackIteration
        {
            EXPECT_EQ(bootstrap.GetOrigin(), Network::RemoteAddress::Origin::Cache);
            EXPECT_NE(std::ranges::find(expectations, bootstrap), expectations.end());
            return CallbackIteration::Continue;
        });
    EXPECT_EQ(read, service.BootstrapCount());
}

//----------------------------------------------------------------------------------------------------------------------

TEST(BootstrapServiceSuite, ParseMalformedFileTest)
{
    auto const spRegistrar = std::make_shared<Scheduler::Registrar>();
    BootstrapService service(local::GetFilepath("malformed/bootstrap.json"));
    service.Register(spRegistrar);
    
    EXPECT_FALSE(service.FetchBootstraps()); // Verify that reading a malformed file will not cause an exception. 
    EXPECT_EQ(service.BootstrapCount(), std::size_t(0));
    EXPECT_EQ(service.BootstrapCount(Network::Protocol::TCP), std::size_t(0));
    std::size_t const read = service.ForEachBootstrap(Network::Protocol::TCP,
        [] (auto const&) -> CallbackIteration { return CallbackIteration::Continue; });
    EXPECT_EQ(read, service.BootstrapCount());
}

//----------------------------------------------------------------------------------------------------------------------

TEST(BootstrapServiceSuite, ParseMissingBootstrapsTest)
{
    auto const spRegistrar = std::make_shared<Scheduler::Registrar>();
    BootstrapService service(local::GetFilepath("missing/bootstrap.json"));
    service.Register(spRegistrar);

    EXPECT_TRUE(service.FetchBootstraps()); // Verify that file with no bootstraps can be read. 
    EXPECT_EQ(service.BootstrapCount(), std::size_t(0));
    EXPECT_EQ(service.BootstrapCount(Network::Protocol::TCP), std::size_t(0));
    std::size_t const read = service.ForEachBootstrap(Network::Protocol::TCP,
        [] (auto const&) -> CallbackIteration { return CallbackIteration::Continue; });
    EXPECT_EQ(read, service.BootstrapCount());
}

//----------------------------------------------------------------------------------------------------------------------

TEST(BootstrapServiceSuite, CacheSearchTest)
{
    auto const spRegistrar = std::make_shared<Scheduler::Registrar>();
    std::filesystem::path const filepath = local::GetFilepath("good/bootstrap.json");
    BootstrapService service(filepath);
    service.Register(spRegistrar);

    ASSERT_TRUE(service.FetchBootstraps());
    EXPECT_EQ(service.BootstrapCount(Network::Protocol::TCP), test::FileStoredCount);

    // Make an address that should be present in the file.
    Network::RemoteAddress const expected = test::BootstrapGenerator(test::TcpBasePort + 1);
    EXPECT_TRUE(service.Contains(expected)); // Verify the fast lookup method. 

    // Veify we can use the for-each method to match an address and stop. 
    bool found = false;
    std::size_t const read = service.ForEachBootstrap(
        Network::Protocol::TCP,
        [&found, &expected] (Network::RemoteAddress const& bootstrap) -> CallbackIteration
        {
            if (bootstrap == expected) { found = true; return CallbackIteration::Stop; }
            return CallbackIteration::Continue;
        });
    EXPECT_TRUE(found);
    EXPECT_LT(read, service.BootstrapCount());
}

//----------------------------------------------------------------------------------------------------------------------

TEST(BootstrapServiceSuite, CacheUpdateTest)
{
    // The number of new bootstraps to be added. 
    constexpr std::size_t ExpectedUpdateCount = 1000;
    constexpr std::uint16_t UpdateLowerBound = test::TcpBasePort + test::FileStoredCount;
    constexpr std::uint16_t UpdateUpperBound = UpdateLowerBound + std::uint16_t{ ExpectedUpdateCount };

    // Random boolean generators for selections ports to sample from the cache.
    std::random_device device;
    std::mt19937 generator{ device() };
    std::bernoulli_distribution sampler{ 0.25 };

    // Create the service to read the good file.
    auto const spRegistrar = std::make_shared<Scheduler::Registrar>();
    std::filesystem::path const filepath = local::GetFilepath("good/bootstrap.json");
    BootstrapService service(filepath);
    service.Register(spRegistrar);

    ASSERT_TRUE(service.FetchBootstraps());

    // Add a series of new bootstraps to the cache stage.
    { 
        std::size_t const countBeforeUpdate = service.BootstrapCount();
        for (auto port : std::views::iota(UpdateLowerBound, UpdateUpperBound)) {
            service.OnRemoteConnected(test::EndpointIdentifier, test::BootstrapGenerator(port));
            EXPECT_EQ(countBeforeUpdate, service.BootstrapCount()); // The cache shouldn't contain the staged updates.
        }

        // Verify the states of the cache after the updates are collated.
        auto const [applied, difference] = service.UpdateCache();
        EXPECT_EQ(applied, ExpectedUpdateCount);
        EXPECT_EQ(difference, ExpectedUpdateCount);
        EXPECT_EQ(service.BootstrapCount(), test::FileStoredCount + ExpectedUpdateCount);
    }

    EXPECT_EQ(service.Serialize(), Configuration::StatusCode::Success); // Verify we can manually serialize the cache.

    // Verify we can read the bootstraps, by simulating the next start of the application.
    {
        auto const spOtherScheduler = std::make_shared<Scheduler::Registrar>();
        BootstrapService verifier(filepath);
        verifier.Register(spOtherScheduler);

        EXPECT_TRUE(verifier.FetchBootstraps());
        EXPECT_EQ(service.BootstrapCount(), test::FileStoredCount + ExpectedUpdateCount);
        EXPECT_EQ(service.BootstrapCount(Network::Protocol::TCP), test::FileStoredCount + ExpectedUpdateCount);

        // Generate a sample. inclusive of the original set, to verify they exist in the cache.
        auto sample = std::views::iota(test::TcpBasePort, UpdateUpperBound) | 
            std::views::filter([&sampler, &generator] (std::uint16_t) { return sampler(generator); });
        for (auto port : sample) { EXPECT_TRUE(verifier.Contains(test::BootstrapGenerator(port))); }

        std::size_t const read = verifier.ForEachBootstrap(
            Network::Protocol::TCP, [] (auto const& ) -> CallbackIteration { return CallbackIteration::Continue; });
        EXPECT_EQ(read, verifier.BootstrapCount());
        EXPECT_EQ(read, test::FileStoredCount + ExpectedUpdateCount);
    }

    // Verify duplicate bootstraps don't affect the cache.
    {
        std::size_t const countBeforeUpdate = service.BootstrapCount();
        auto sample = std::views::iota(UpdateLowerBound, UpdateUpperBound) | 
            std::views::filter([&sampler, &generator] (std::uint16_t) { return sampler(generator); });
        std::size_t updates = 0;
        for (auto port : sample) {
            service.OnRemoteConnected(test::EndpointIdentifier, test::BootstrapGenerator(port));
            ++updates;
        }

        // Verify the states of the cache after the updates are collated.
        auto const [applied, difference] = service.UpdateCache();
        EXPECT_EQ(applied, updates);
        EXPECT_EQ(difference, std::size_t(0));
        EXPECT_EQ(service.BootstrapCount(), countBeforeUpdate);
    }

    // Remove the series of new bootstraps to the cache stage.
    {
        std::size_t const countBeforeUpdate = service.BootstrapCount();
        for (auto port : std::views::iota(UpdateLowerBound, UpdateUpperBound)) {
            service.OnRemoteDisconnected(test::EndpointIdentifier, test::BootstrapGenerator(port));
            EXPECT_EQ(countBeforeUpdate, service.BootstrapCount()); // The cache shouldn't contain the staged updates.
        }

        // Verify the states of the cache after the updates are collated.
        auto const [applied, difference] = service.UpdateCache();
        EXPECT_EQ(applied, ExpectedUpdateCount);
        EXPECT_EQ(difference, -ExpectedUpdateCount); 
        EXPECT_EQ(service.BootstrapCount(), test::FileStoredCount);

        // Verify we can no longer find the bootstraps after removal. 
        auto sample = std::views::iota(UpdateLowerBound, UpdateUpperBound) | 
            std::views::filter([&sampler, &generator] (std::uint16_t) { return sampler(generator); });
        for (auto port : sample) { EXPECT_FALSE(service.Contains(test::BootstrapGenerator(port))); }
    }
    
    // Verify the cache is back to the initial state.
    for (auto port : std::views::iota(test::TcpBasePort, test::TcpBasePort + test::FileStoredCount)) {
        EXPECT_TRUE(service.Contains(test::BootstrapGenerator(port)));
    }

    // Verify the file has been reset to its original state.
    EXPECT_EQ(service.Serialize(), Configuration::StatusCode::Success); 
}

//----------------------------------------------------------------------------------------------------------------------

std::filesystem::path local::GetFilepath(std::filesystem::path const& filename)
{
    auto path = std::filesystem::current_path();
    if (path.filename() == "UT_Configuration") { 
        return path / "files" / filename;
    } else {
        if (path.filename() == "bin") { path.remove_filename(); }
        return path / "Tests/UT_Configuration/files" / filename;
    }
}

//----------------------------------------------------------------------------------------------------------------------

Configuration::Options::Endpoint local::GenerateTcpOptions(std::uint16_t port)
{
    constexpr auto RuntimeOptions = Configuration::Options::Runtime
    { 
        .context = RuntimeContext::Foreground,
        .verbosity = spdlog::level::debug,
        .useInteractiveConsole = false,
        .useBootstraps = false,
        .useFilepathDeduction = false
    };

    Configuration::Options::Endpoint options;
    options.protocol = "TCP";
    options.interface = "lo";
    options.binding = test::TcpBootstrapBase.data() + std::to_string(port);
    options.bootstrap = test::TcpBootstrapBase.data() + std::to_string(port);
    [[maybe_unused]] bool const success = options.Initialize(RuntimeOptions, spdlog::get(Logger::Name::Core.data()));
    assert(success);
    return options;
}

//----------------------------------------------------------------------------------------------------------------------
