//----------------------------------------------------------------------------------------------------------------------
#include <brypt/brypt.hpp>
//----------------------------------------------------------------------------------------------------------------------
#include <gtest/gtest.h>
//----------------------------------------------------------------------------------------------------------------------
#include <string>
#include <thread>
#include <filesystem>
//----------------------------------------------------------------------------------------------------------------------
using namespace std::chrono_literals;
//----------------------------------------------------------------------------------------------------------------------

//----------------------------------------------------------------------------------------------------------------------
namespace {
namespace local {
//----------------------------------------------------------------------------------------------------------------------

class EventObserver;

//----------------------------------------------------------------------------------------------------------------------
} // local namespace
//----------------------------------------------------------------------------------------------------------------------
namespace test {
//----------------------------------------------------------------------------------------------------------------------

using IntegerExpectations = std::vector<std::pair<std::int32_t, bool>>;
using MillisecondExpectations = std::vector<std::pair<std::chrono::milliseconds, bool>>;
using StringExpectations = std::vector<std::pair<std::string, bool>>;

//----------------------------------------------------------------------------------------------------------------------
} // local namespace
} // namespace
//----------------------------------------------------------------------------------------------------------------------

class local::EventObserver
{
public:
    enum class Watching : std::uint32_t { Service, Target };

    using EventEntry = std::pair<brypt::event, bool>;
    using EventRecord = std::vector<EventEntry>;
    explicit EventObserver(Watching watching);
    [[nodiscard]] bool Subscribe(brypt::service& service);
    [[nodiscard]] bool IsSequenceExpected() const;

private:
    static constexpr std::uint32_t ExpectedEventCount = 6; // The number of events each endpoint should fire. 

    [[nodiscard]] bool IsServiceSequenceExpected() const;
    [[nodiscard]] bool IsTargetSequenceExpected() const;

    Watching m_watching;
    EventRecord m_record;
};

//----------------------------------------------------------------------------------------------------------------------

class LibraryServiceSuite : public testing::Test
{
protected:
    static void SetUpTestSuite()
    {
        constexpr auto GetTestFilepath = [] () -> std::filesystem::path {
            auto path = std::filesystem::current_path();
            if (path.filename() == "UT_BryptLibrary") { 
                path /= "config";
            } else {
#if defined(WIN32)
                path = path.parent_path();
#endif
                if (path.filename() == "bin") { path.remove_filename(); }
                path /= "Tests/UT_BryptLibrary/config";
            }
            return path;
        };
        
        m_filepath = GetTestFilepath();

        {
            brypt::result result;
            m_upService = std::make_unique<brypt::service>(m_filepath.string().c_str(), result);
            ASSERT_TRUE(result.is_success());
        }

        {
            brypt::result result;
            m_upTarget = std::make_unique<brypt::service>();
            ASSERT_TRUE(result.is_success());
        }

        // Set the configuration fields that will be required by multiple tests. 
        {
            // Verify that the base filepath is set to the value provided at construction. 
            auto const option = m_upService->get_option(brypt::option::base_path);
            ASSERT_EQ(option.value<std::string>(), m_filepath.string());

            // Reset the base filepath for the rest of the tests. 
            auto const result = m_upService->set_option({ brypt::option::base_path, m_filepath.string().c_str() });
            ASSERT_TRUE(result.is_success());
        }

        {
            // Verify that the configuration filename is set to the expected default value.
            auto const option = m_upService->get_option(brypt::option::configuration_filename);
            ASSERT_EQ(option.value<std::string>(), "brypt.config.json");
            
            // Reset the configuration filename for the rest of the tests. 
            auto const result = m_upService->set_option({ brypt::option::configuration_filename, "config.json" });
            ASSERT_TRUE(result.is_success());
        }

        {
            // Verify that the bootstrap filename is set to the expected default value.
            auto const option = m_upService->get_option(brypt::option::bootstrap_filename);
            ASSERT_EQ(option.value<std::string>(), "brypt.bootstrap.json");

            // Reset the bootstrap filename for the rest of the tests. 
            auto const result = m_upService->set_option({ brypt::option::bootstrap_filename, "bootstrap.json" });
            ASSERT_TRUE(result.is_success());
        }
    }

    static void TearDownTestSuite()
    {
        m_upTarget.reset();
        m_upService.reset();

        // Files should not have been generated with the default filenames. 
        EXPECT_FALSE(std::filesystem::exists(m_filepath / "brypt.config.json"));
        EXPECT_FALSE(std::filesystem::exists(m_filepath / "brypt.bootstrap.json"));

        // A bootstraps file for the custom name should have been generated, but we should remove it for 
        // future tests. 
        auto const bootstraps = m_filepath / "bootstrap.json";
        EXPECT_TRUE(std::filesystem::exists(bootstraps));
        std::filesystem::remove(bootstraps);
        EXPECT_FALSE(std::filesystem::exists(bootstraps));
    }

    void SetUp() override { }
    void TearDown() override { }

    static std::filesystem::path m_filepath;
    static std::unique_ptr<brypt::service> m_upTarget;
    static std::unique_ptr<brypt::service> m_upService;
    static std::vector<std::unique_ptr<local::EventObserver>> m_observers;
    static bool m_hasReceivedEventAfterSuspension;
};

//----------------------------------------------------------------------------------------------------------------------

std::filesystem::path LibraryServiceSuite::m_filepath = {};
std::unique_ptr<brypt::service> LibraryServiceSuite::m_upTarget = nullptr;
std::unique_ptr<brypt::service> LibraryServiceSuite::m_upService = nullptr;
std::vector<std::unique_ptr<local::EventObserver>> LibraryServiceSuite::m_observers = {};
bool LibraryServiceSuite::m_hasReceivedEventAfterSuspension = false;

//----------------------------------------------------------------------------------------------------------------------

TEST_F(LibraryServiceSuite, BaseFilepathOptionTest)
{
    std::string(1024, 'a');
    auto const pwd = std::filesystem::current_path().string();
    test::StringExpectations const expectations = {
        { pwd, true },
        { "", true },
        { "/temporary", false },
        { pwd + "/doesnotexist/", false }, 
        { pwd + "/\0temporary\0" + pwd , false }, 
        { std::string(1024, '0') + pwd , false },
        { pwd + std::string(1024, '0') , false },
        { "temporary.json", false },
        { "/temporary.json", false },
        { pwd + "/temporary.json", false }, 
        { "/temporary/temporary.json", false },
        { "temporary.json\0temporary.json", false },
        { "temporary", false },
        { "\\/:\"*?<>|", false },
        { "\\/:\"*?<>|.json", false },
    };

    // Verify that the set of possible inputs match the expected setter and getter results. 
    for (auto const& [input, validity] : expectations) {
        auto const result = m_upService->set_option(brypt::option::base_path, input);
        EXPECT_EQ(result.is_success(), validity);

        auto const value = m_upService->get_option<std::string>(brypt::option::base_path);
        EXPECT_EQ(value, validity ? input : "");
    }

    // Reset the base filepath for the rest of the tests. 
    auto const result = m_upService->set_option({ brypt::option::base_path, m_filepath.string().c_str()});
    ASSERT_TRUE(result.is_success());
}

//----------------------------------------------------------------------------------------------------------------------

TEST_F(LibraryServiceSuite, ConfigurationFilenameOptionTest)
{
    auto const pwd = m_filepath.string();
    std::string const extension = ".json";

    #if defined (_WIN32)
    std::size_t limit = 255 - pwd.size() - extension.size();
    #else 
    std::size_t limit = 255 - extension.size();
    #endif

    test::StringExpectations const expectations = {
        { "temporary.json", true },
        { "", true },
        { "temporary.cfg", false },
        { "temporary.png", false },
        { std::string(limit, '0') + extension, true },
        { std::string(limit + 1, '0') + extension, false },
        { std::string(1024, '0') + extension, false },
        { "/temporary.json", false },
        { "/temporary", false },
        { pwd + "/temporary.json", false }, 
        { "/temporary/temporary.json", false },
        { "temporary", false },
        { "\\/:\"*?<>|", false },
        { "\\/:\"*?<>|.json", false },
    };

    // Verify that the set of possible inputs match the expected setter and getter results. 
    for (auto const& [input, validity] : expectations) {
        auto const result = m_upService->set_option(brypt::option::configuration_filename, input);
        EXPECT_EQ(result.is_success(), validity);

        auto const value = m_upService->get_option<std::string>(brypt::option::configuration_filename);
        EXPECT_EQ(value, validity ? input : "");
    }

    // Reset the configuration filename for the rest of the tests. 
    auto const result = m_upService->set_option({ brypt::option::configuration_filename, "config.json" });
    ASSERT_TRUE(result.is_success());
}

//----------------------------------------------------------------------------------------------------------------------

TEST_F(LibraryServiceSuite, BootstrapFilenameOptionTest)
{
    auto const pwd = m_filepath.string();
    std::string const extension = ".json";

    #if defined (_WIN32)
    std::size_t limit = 255 - pwd.size() - extension.size();
    #else 
    std::size_t limit = 255 - extension.size();
    #endif

    test::StringExpectations const expectations = {
        { "temporary.json", true },
        { "", true },
        { "temporary.cfg", false },
        { "temporary.png", false },
        { std::string(limit, '0') + extension, true },
        { std::string(limit + 1, '0') + extension, false },
        { std::string(1024, '0') + extension, false },
        { "/temporary.json", false },
        { "/temporary", false },
        { pwd + "/temporary.json", false }, 
        { "/temporary/temporary.json", false },
        { "temporary", false },
        { "\\/:\"*?<>|", false },
        { "\\/:\"*?<>|.json", false },
    };

    // Verify that the set of possible inputs match the expected setter and getter results. 
    for (auto const& [input, validity] : expectations) {
        auto const result = m_upService->set_option(brypt::option::bootstrap_filename, input);
        EXPECT_EQ(result.is_success(), validity);

        auto const value = m_upService->get_option<std::string>(brypt::option::bootstrap_filename);
        EXPECT_EQ(value, validity ? input : "");
    }
    
    // Reset the bootstrap filename for the rest of the tests. 
    auto const result = m_upService->set_option({ brypt::option::bootstrap_filename, "bootstrap.json" });
    ASSERT_TRUE(result.is_success());
}

//----------------------------------------------------------------------------------------------------------------------

TEST_F(LibraryServiceSuite, CoreThreadsOptionTest)
{
    test::IntegerExpectations const expectations = {
        { 0, true },
        { 1, true },
        { 2, false },
        { std::numeric_limits<std::int32_t>::min(), false },
        { std::numeric_limits<std::int32_t>::max(), false },
    };

    // Verify that service is configured to run in the background by default.
    {
        auto const option = m_upService->get_option(brypt::option::core_threads);
        ASSERT_TRUE(option.has_value() && option.contains<std::int32_t>());
        ASSERT_EQ(option.value<std::int32_t>(), 1);
    }

    // Verify that the set of possible inputs match the expected setter and getter results. 
    for (auto const& [input, validity] : expectations) {
        auto const result = m_upService->set_option(brypt::option::core_threads, input);
        EXPECT_EQ(result.is_success(), validity);

        auto const value = m_upService->get_option<std::int32_t>(brypt::option::core_threads);
        EXPECT_EQ(value == input, validity); // Note: The value returned should be set to the last valid value.
    }

    // Reset the core threads to the expected value. 
    auto const result = m_upService->set_option({ brypt::option::core_threads, 1 });
    ASSERT_TRUE(result.is_success());
}

//----------------------------------------------------------------------------------------------------------------------

TEST_F(LibraryServiceSuite, IdentifierOptionTest)
{
    test::IntegerExpectations const expectations = {
        { static_cast<std::int32_t>(brypt::identifier_type::ephemeral), true },
        { static_cast<std::int32_t>(brypt::identifier_type::persistent), true },
        { static_cast<std::int32_t>(brypt::identifier_type::ephemeral) - 1, false },
        { static_cast<std::int32_t>(brypt::identifier_type::persistent) + 1, false },
        { std::numeric_limits<std::int32_t>::min(), false },
        { std::numeric_limits<std::int32_t>::max(), false },
    };

    // Verify that service has disabled logging by default.
    {
        auto const option = m_upService->get_option(brypt::option::identifier_type);
        ASSERT_TRUE(option.has_value() && option.contains<brypt::identifier_type>());
        ASSERT_EQ(option.value<brypt::identifier_type>(), brypt::identifier_type::persistent);
    }

    auto const identifier = m_upService->get_identifier();
    EXPECT_TRUE(identifier.size() >= BRYPT_IDENTIFIER_MIN_SIZE && identifier.size() <= BRYPT_IDENTIFIER_MAX_SIZE);

    // Verify that the set of possible inputs match the expected setter and getter results. 
    for (auto const& [input, validity] : expectations) {
        auto const result = m_upService->set_option(brypt::option::identifier_type, input);
        EXPECT_EQ(result.is_success(), validity);
        if (result.is_success()) { EXPECT_NE(identifier, m_upService->get_identifier()); }

        // If the value to be set was invalid, the value returned should be set to the last valid value. 
        auto const value = m_upService->get_option<brypt::identifier_type>(brypt::option::identifier_type);
        EXPECT_EQ(static_cast<std::int32_t>(value) == input, validity); 
    }

    // Reset the identifier type for the rest of the tests. 
    auto const result = m_upService->set_option({ brypt::option::identifier_type, brypt::identifier_type::persistent });
    ASSERT_TRUE(result.is_success());
}

//----------------------------------------------------------------------------------------------------------------------

TEST_F(LibraryServiceSuite, UseBootstrapsOptionTest)
{
    // Verify that service is configured to use the bootstraps stored in the cache file by default. 
    {
        auto const option = m_upService->get_option(brypt::option::use_bootstraps);
        ASSERT_TRUE(option.value<bool>());
    }

    // Verify we can toggle the use bootstraps option value. 
    {
        auto const result = m_upService->set_option(brypt::option::use_bootstraps, false);
        EXPECT_TRUE(result.is_success());

        auto const value = m_upService->get_option<bool>(brypt::option::use_bootstraps);
        EXPECT_FALSE(value);
    }

    // Reset bootstraps usage for the rest of the tests. 
    auto const result = m_upService->set_option({ brypt::option::use_bootstraps, true });
    ASSERT_TRUE(result.is_success());
}

//----------------------------------------------------------------------------------------------------------------------

TEST_F(LibraryServiceSuite, NodeNameOptionTest)
{
    constexpr std::size_t limit = 64;
    test::StringExpectations const expectations = {
        { "", true },
        { "test_name", true },
        { std::string(limit, '0'), true },
        { std::string(limit + 1, '0'), false },
        { std::string(1024, '0'), false },
    };

    // Verify that service has disabled logging by default.
    {
        auto const option = m_upService->get_option(brypt::option::node_name);
        ASSERT_TRUE(option.has_value() && option.contains<std::string>());
        ASSERT_EQ(option.value<std::string>(), "name");
    }

    // Verify that the set of possible inputs match the expected setter and getter results. 
    for (auto const& [input, validity] : expectations) {
        auto const result = m_upService->set_option(brypt::option::node_name, input);
        EXPECT_EQ(result.is_success(), validity);

        auto const value = m_upService->get_option<std::string>(brypt::option::node_name);
        EXPECT_EQ(value == input, validity);
    }

    // Reset the node name for the rest of the tests. 
    auto const result = m_upService->set_option({ brypt::option::node_name, "name" });
    ASSERT_TRUE(result.is_success());
}

//----------------------------------------------------------------------------------------------------------------------

TEST_F(LibraryServiceSuite, NodeDescriptionOptionTest)
{
    constexpr std::size_t limit = 256;
    test::StringExpectations const expectations = {
        { "", true },
        { "test_description", true },
        { std::string(limit, '0'), true },
        { std::string(limit + 1, '0'), false },
        { std::string(1024, '0'), false },
    };

    // Verify that service has disabled logging by default.
    {
        auto const option = m_upService->get_option(brypt::option::node_description);
        ASSERT_TRUE(option.has_value() && option.contains<std::string>());
        ASSERT_EQ(option.value<std::string>(), "description");
    }

    // Verify that the set of possible inputs match the expected setter and getter results. 
    for (auto const& [input, validity] : expectations) {
        auto const result = m_upService->set_option(brypt::option::node_description, input);
        EXPECT_EQ(result.is_success(), validity);

        auto const value = m_upService->get_option<std::string>(brypt::option::node_description);
        EXPECT_EQ(value == input, validity);
    }

    // Reset the node description for the rest of the tests. 
    auto const result = m_upService->set_option({ brypt::option::node_description, "description" });
    ASSERT_TRUE(result.is_success());
}

//----------------------------------------------------------------------------------------------------------------------

TEST_F(LibraryServiceSuite, SecurityStrategyOptionTest)
{
    test::IntegerExpectations const expectations = {
        { static_cast<std::int32_t>(brypt::security_strategy::pqnistl3), true },
        { static_cast<std::int32_t>(brypt::security_strategy::pqnistl3) - 1, false },
        { static_cast<std::int32_t>(brypt::security_strategy::pqnistl3) + 1, false },
        { std::numeric_limits<std::int32_t>::min(), false },
        { std::numeric_limits<std::int32_t>::max(), false },
    };

    // Verify that service has disabled logging by default.
    {
        auto const option = m_upService->get_option(brypt::option::security_strategy);
        ASSERT_TRUE(option.has_value() && option.contains<brypt::security_strategy>());
        ASSERT_EQ(option.value<brypt::security_strategy>(), brypt::security_strategy::pqnistl3);
    }

    // Verify that the set of possible inputs match the expected setter and getter results. 
    for (auto const& [input, validity] : expectations) {
        auto const result = m_upService->set_option(brypt::option::security_strategy, input);
        EXPECT_EQ(result.is_success(), validity);

        auto const value = m_upService->get_option<brypt::security_strategy>(brypt::option::security_strategy);
        EXPECT_EQ(static_cast<std::int32_t>(value) == input, validity);
    }

    // Reset the security strategy for the rest of the tests. 
    auto const result = m_upService->set_option(
        { brypt::option::security_strategy, brypt::security_strategy::pqnistl3 });
    ASSERT_TRUE(result.is_success());
}

//----------------------------------------------------------------------------------------------------------------------

TEST_F(LibraryServiceSuite, LogLevelOptionTest)
{
    test::IntegerExpectations const expectations = {
        { static_cast<std::int32_t>(brypt::log_level::trace), true },
        { static_cast<std::int32_t>(brypt::log_level::debug), true },
        { static_cast<std::int32_t>(brypt::log_level::info), true },
        { static_cast<std::int32_t>(brypt::log_level::warn), true },
        { static_cast<std::int32_t>(brypt::log_level::err), true },
        { static_cast<std::int32_t>(brypt::log_level::critical), true },
        { static_cast<std::int32_t>(brypt::log_level::off), true },
        { static_cast<std::int32_t>(brypt::log_level::off) - 1, false },
        { static_cast<std::int32_t>(brypt::log_level::critical) + 1, false },
        { std::numeric_limits<std::int32_t>::min(), false },
        { std::numeric_limits<std::int32_t>::max(), false },
    };

    // Verify that service has disabled logging by default.
    {
        auto const option = m_upService->get_option(brypt::option::log_level);
        ASSERT_TRUE(option.has_value() && option.contains<brypt::log_level>());
        ASSERT_EQ(option.value<brypt::log_level>(), brypt::log_level::off);
    }

     // Verify that the set of possible inputs match the expected setter and getter results. 
     for (auto const& [input, validity] : expectations) {
         auto const result = m_upService->set_option(brypt::option::log_level, input);
         EXPECT_EQ(result.is_success(), validity);

         auto const value = m_upService->get_option<brypt::log_level>(brypt::option::log_level);
         EXPECT_EQ(static_cast<std::int32_t>(value), validity ? input : std::int32_t{});
     }

     // Reset the log level for the rest of the tests. 
     auto const result = m_upService->set_option({ brypt::option::log_level, brypt::log_level::off });
     ASSERT_TRUE(result.is_success());
}

//----------------------------------------------------------------------------------------------------------------------

TEST_F(LibraryServiceSuite, ConnectionTimeoutOptionTest)
{
    test::MillisecondExpectations const expectations = {
        { 0ms, true },
        { 1ms, true },
        { 125ms, true },
        { 1000ms, true },
        { 1000s, true },
        { 1000min, true },
        { 24h, true },
        { 25h, false },
        { -1ms, false },
        { std::chrono::milliseconds::min(), false },
        { std::chrono::milliseconds::max(), false },
    };

    // Verify that service is configured to run in the background by default.
    {
        auto const option = m_upService->get_option(brypt::option::connection_timeout);
        ASSERT_TRUE(option.has_value() && option.contains<std::chrono::milliseconds>());
        ASSERT_EQ(option.value<std::chrono::milliseconds>(), 250ms);
    }

    // Verify that the set of possible inputs match the expected setter and getter results. 
    for (auto const& [input, validity] : expectations) {
        auto const result = m_upService->set_option(brypt::option::connection_timeout, input);
        EXPECT_EQ(result.is_success(), validity);

        auto const value = m_upService->get_option<std::int32_t>(brypt::option::connection_timeout);
        EXPECT_EQ(value == input.count(), validity); // Note: The value returned should be set to the last valid value.
    }

    // Reset the core threads to the expected value. 
    auto const result = m_upService->set_option({ brypt::option::connection_timeout, 250ms });
    ASSERT_TRUE(result.is_success());
}

//----------------------------------------------------------------------------------------------------------------------

TEST_F(LibraryServiceSuite, ConnectionRetryLimitOptionTest)
{
    test::IntegerExpectations const expectations = {
        { 0, true },
        { 1, true },
        { 5, true },
        { 100, true },
        { std::numeric_limits<std::int32_t>::max(), true },
        { -1, false },
        { std::numeric_limits<std::int32_t>::min(), false },
    };

    // Verify that service is configured to run in the background by default.
    {
        auto const option = m_upService->get_option(brypt::option::connection_retry_limit);
        ASSERT_TRUE(option.has_value() && option.contains<std::int32_t>());
        ASSERT_EQ(option.value<std::int32_t>(), 5);
    }

    // Verify that the set of possible inputs match the expected setter and getter results. 
    for (auto const& [input, validity] : expectations) {
        auto const result = m_upService->set_option(brypt::option::connection_retry_limit, input);
        EXPECT_EQ(result.is_success(), validity);

        auto const value = m_upService->get_option<std::int32_t>(brypt::option::connection_retry_limit);
        EXPECT_EQ(value == input, validity); // Note: The value returned should be set to the last valid value.
    }

    // Reset the core threads to the expected value. 
    auto const result = m_upService->set_option({ brypt::option::connection_retry_limit, 5 });
    ASSERT_TRUE(result.is_success());
}

//----------------------------------------------------------------------------------------------------------------------

TEST_F(LibraryServiceSuite, ConnectionRetryIntervalOptionTest)
{
    test::MillisecondExpectations const expectations = {
        { 0ms, true },
        { 1ms, true },
        { 125ms, true },
        { 1000ms, true },
        { 1000s, true },
        { 1000min, true },
        { 24h, true },
        { 25h, false },
        { -1ms, false },
        { std::chrono::milliseconds::min(), false },
        { std::chrono::milliseconds::max(), false },
    };

    // Verify that service is configured to run in the background by default.
    {
        auto const option = m_upService->get_option(brypt::option::connection_retry_interval);
        ASSERT_TRUE(option.has_value() && option.contains<std::chrono::milliseconds>());
        ASSERT_EQ(option.value<std::chrono::milliseconds>(), 100ms);
    }

    // Verify that the set of possible inputs match the expected setter and getter results. 
    for (auto const& [input, validity] : expectations) {
        auto const result = m_upService->set_option(brypt::option::connection_retry_interval, input);
        EXPECT_EQ(result.is_success(), validity);

        auto const value = m_upService->get_option<std::int32_t>(brypt::option::connection_retry_interval);
        EXPECT_EQ(value == input.count(), validity); // Note: The value returned should be set to the last valid value.
    }

    // Reset the core threads to the expected value. 
    auto const result = m_upService->set_option({ brypt::option::connection_retry_interval, 100ms });
    ASSERT_TRUE(result.is_success());
}

//----------------------------------------------------------------------------------------------------------------------

TEST_F(LibraryServiceSuite, EndpointOptionTest)
{
    using EndpointExpectations = std::vector<std::tuple<brypt::endpoint_options, bool>>;
    EndpointExpectations expectations = {
        { { brypt::protocol::tcp, "lo", "*:35217" }, true },
        { { brypt::protocol::tcp, "lo", "*:35217", "127.0.0.1:35217" }, true },
        { { brypt::protocol::tcp, "lo", "127.0.0.1:35217", "127.0.0.1:35217" }, true },
        { { brypt::protocol::unknown, "lo", "*:35217" }, false },
        { { brypt::protocol::unknown, "lo", "*:35217", "127.0.0.1:35217" }, false },
        { { brypt::protocol::tcp, "lo", "abcd" }, false },
        { { brypt::protocol::tcp, "lo", "abcd", "127.0.0.1:35217" }, false },
        { { brypt::protocol::tcp, "lo", "*:35217", "abcd" }, false },
        { { brypt::protocol::unknown, "abcd", "abcd", "abcd" }, false },
        { { brypt::protocol::tcp, "", "", "" }, false },
        { { brypt::protocol::unknown, "", "" }, false },
        { { brypt::protocol::unknown, "", "", "" }, false }
    };

    // Verify the initial endpoint configuration matches the values set in the configuration file.
    {
        auto const endpooints = m_upService->get_endpoints();
        ASSERT_EQ(endpooints.size(), 1);
        brypt::endpoint_options const& options = endpooints.front();
        EXPECT_EQ(options.get_protocol(), brypt::protocol::tcp);
        EXPECT_EQ(options.get_interface(), "lo");
        EXPECT_EQ(options.get_binding(), "127.0.0.1:35217");
        ASSERT_TRUE(options.get_bootstrap());
        EXPECT_EQ(options.get_bootstrap().value(), "127.0.0.1:35216");
    }

    // Verify we can update the endpoint options to add a bootstrap. 
    {
        EXPECT_TRUE(m_upService->attach_endpoint({ brypt::protocol::tcp, "lo", "127.0.0.1:35217" }));
        EXPECT_EQ(m_upService->get_endpoints().size(), 1);

        auto const optOptions = m_upService->find_endpoint(brypt::protocol::tcp, "127.0.0.1:35217");
        ASSERT_TRUE(optOptions);
        EXPECT_EQ(optOptions->get_protocol(), brypt::protocol::tcp);
        EXPECT_EQ(optOptions->get_interface(), "lo");
        EXPECT_EQ(optOptions->get_binding(), "127.0.0.1:35217");
        EXPECT_FALSE(optOptions->get_bootstrap());
    }

    // Verify we can add a second configuration and get both out from the service. 
    {
        EXPECT_TRUE(m_upService->attach_endpoint({ brypt::protocol::tcp, "lo", "127.0.0.1:35218", "127.0.0.1:35219" }));

        auto const endpoints = m_upService->get_endpoints();
        EXPECT_EQ(endpoints.size(), 2);
        
        EXPECT_NE(std::ranges::find_if(
            endpoints, [] (auto const& options) { return options.get_binding() == "127.0.0.1:35217"; }), endpoints.end());
        EXPECT_NE(std::ranges::find_if(
            endpoints, [] (auto const& options) { return options.get_binding() == "127.0.0.1:35218"; }), endpoints.end());
    }

    // Remove the two endpoint options to test the series of expectations from a clean slate. 
    {
        ASSERT_TRUE(m_upService->detach_endpoint(brypt::protocol::tcp, "127.0.0.1:35217"));
        ASSERT_TRUE(m_upService->detach_endpoint(brypt::protocol::tcp, "127.0.0.1:35218"));
        EXPECT_EQ(m_upService->get_endpoints().size(), 0);
        EXPECT_FALSE(m_upService->find_endpoint(brypt::protocol::tcp, "127.0.0.1:35217"));
    }

    // Verify that the set of possible inputs match the expected setter and getter results. 
    for (auto const& [input, validity] : expectations) {
        {
            auto const result = m_upService->attach_endpoint(input);
            EXPECT_EQ(result.is_success(), validity);
        }

        auto const optOptions = m_upService->find_endpoint(brypt::protocol::tcp, input.get_binding());
        EXPECT_EQ(optOptions.has_value(), validity);

        if (optOptions) {
            EXPECT_EQ(optOptions->get_protocol(), input.get_protocol());
            EXPECT_EQ(optOptions->get_interface(), input.get_interface());
            EXPECT_EQ(optOptions->get_binding(), input.get_binding());
            EXPECT_EQ(optOptions->get_bootstrap().value_or(""), input.get_bootstrap().value_or(""));
        }

        {
            auto const result = m_upService->detach_endpoint(input.get_protocol(), input.get_binding());
            EXPECT_EQ(result.is_success(), validity);
        }
    }

    // Reset the endpoint configuration for the rest of the tests. 
    ASSERT_TRUE(m_upService->attach_endpoint({ brypt::protocol::tcp, "lo", "127.0.0.1:35217", "127.0.0.1:35216" }));
}

//----------------------------------------------------------------------------------------------------------------------

TEST_F(LibraryServiceSuite, DefaultConfigurationTest)
{
    using enum brypt::option::runtime_options;
    using enum brypt::option::serialized_options;

    EXPECT_EQ(m_upTarget->get_option<brypt::option::base_path_t>(base_path), "");
    EXPECT_EQ(m_upTarget->get_option<brypt::option::configuration_filename_t>(configuration_filename), "");
    EXPECT_EQ(m_upTarget->get_option<brypt::option::bootstrap_filename_t>(bootstrap_filename), "");
    EXPECT_EQ(m_upTarget->get_option<brypt::option::core_threads_t>(core_threads), 1);
    EXPECT_EQ(
        m_upTarget->get_option<brypt::option::identifier_type_t>(identifier_type),
        brypt::identifier_type::ephemeral);
    EXPECT_EQ(m_upTarget->get_option<brypt::option::use_bootstraps_t>(use_bootstraps), true);
    EXPECT_EQ(m_upTarget->get_option<brypt::option::node_name_t>(node_name), "");
    EXPECT_EQ(m_upTarget->get_option<brypt::option::node_description_t>(node_description), "");
    EXPECT_EQ(
        m_upTarget->get_option<brypt::option::security_strategy_t>(security_strategy),
        brypt::security_strategy::pqnistl3);
    EXPECT_EQ(m_upTarget->get_option<brypt::option::log_level_t>(log_level), brypt::log_level::off);
    EXPECT_EQ(m_upTarget->get_option<brypt::option::connection_timeout_t>(connection_timeout), 15'000ms);
    EXPECT_EQ(m_upTarget->get_option<brypt::option::connection_retry_limit_t>(connection_retry_limit), 3);
    EXPECT_EQ(
        m_upTarget->get_option<brypt::option::connection_retry_interval_t>(connection_retry_interval), 5'000ms);

    EXPECT_NE(m_upTarget->get_identifier(), "");
    EXPECT_NE(m_upTarget->get_identifier(), m_upService->get_identifier());
}

//----------------------------------------------------------------------------------------------------------------------

TEST_F(LibraryServiceSuite, ServiceLifecycleTest)
{
    auto const& serviceIdentifier = m_upService->get_identifier();
    auto const& targetIdentifier = m_upTarget->get_identifier();

    // Subscribe to events provided through the service. 
    {
        using enum local::EventObserver::Watching;
        auto& upFirstObserver = m_observers.emplace_back(std::make_unique<local::EventObserver>(Service));
        EXPECT_TRUE(upFirstObserver->Subscribe(*m_upService));

        auto& upSecondObserver = m_observers.emplace_back(std::make_unique<local::EventObserver>(Service));
        EXPECT_TRUE(upSecondObserver->Subscribe(*m_upService)); // Verify we can subscribe to all events more than once.

        auto& upTargetObserver = m_observers.emplace_back(std::make_unique<local::EventObserver>(Target));
        EXPECT_TRUE(upTargetObserver->Subscribe(*m_upTarget));
    }

    // Capture and verify the log output.
    {
        EXPECT_TRUE(m_upService->set_option(brypt::option::log_level, brypt::log_level::info));
        auto const attached = m_upService->register_logger([&] (brypt::log_level level, std::string_view message) {
            EXPECT_GE(static_cast<std::int32_t>(level), static_cast<std::int32_t>(brypt::log_level::info));
            EXPECT_FALSE(message.empty());
        });
        EXPECT_TRUE(attached.is_success());
    }

    // Setup the messaging routes on the service and target.
    {
        auto const setup = m_upTarget->route(
            "/ping", [&] (std::string_view source, std::span<uint8_t const> payload, brypt::next const& next) {
                EXPECT_EQ(source, serviceIdentifier);
                EXPECT_EQ(brypt::helpers::to_string_view(payload), "dispatch");
                auto const result = next.dispatch("/pong", brypt::helpers::marshall("dispatch"));
                EXPECT_TRUE(result.is_success());
                return result.is_success();
            });
        EXPECT_TRUE(setup.is_success());
    }

    {
        auto const setup = m_upTarget->route(
            "/query", [&] (std::string_view source, std::span<uint8_t const> payload, brypt::next const& next) {
                EXPECT_EQ(source, serviceIdentifier);
                EXPECT_EQ(brypt::helpers::to_string_view(payload), "request");
                auto const result = next.respond(brypt::helpers::marshall("response"), brypt::status_code::ok);
                EXPECT_TRUE(result.is_success());
                return result.is_success();
            });
        EXPECT_TRUE(setup.is_success());
    }

    {
        auto const setup = m_upTarget->route(
            "/rejecting", [&](std::string_view source, std::span<uint8_t const> payload, brypt::next const& next) {
                EXPECT_EQ(source, serviceIdentifier);
                EXPECT_EQ(brypt::helpers::to_string_view(payload), "request");
                auto const result = next.respond(brypt::helpers::marshall("rejected"), brypt::status_code::bad_request);
                EXPECT_TRUE(result.is_success());
                return result.is_success();
            });
        EXPECT_TRUE(setup.is_success());
    }

    {
        auto const setup = m_upService->route(
            "/pong", [&] (std::string_view source, std::span<uint8_t const> payload, brypt::next const&) {
                EXPECT_EQ(source, targetIdentifier);
                EXPECT_EQ(brypt::helpers::to_string_view(payload), "dispatch");
                return true;
            });
        EXPECT_TRUE(setup.is_success());
    }

    EXPECT_TRUE(m_upTarget->set_option(brypt::option::connection_timeout, 250ms));
    EXPECT_TRUE(m_upTarget->set_option(brypt::option::connection_retry_interval, 100ms));

    // Start the primary and target services to begin testing functionality. 
    EXPECT_TRUE(m_upService->startup());
    EXPECT_TRUE(m_upTarget->startup());
    std::this_thread::sleep_for(500ms);

    // Verify the service does not know about the other peer until they have been connected.
    EXPECT_FALSE(m_upTarget->is_peer_connected(targetIdentifier));
    EXPECT_FALSE(m_upService->is_peer_connected(serviceIdentifier));
    EXPECT_FALSE(m_upService->get_peer_statistics(serviceIdentifier));
    EXPECT_FALSE(m_upService->get_peer_details(serviceIdentifier));

    // Verify we can attach an endpoint and schedule a connect while running. 
    ASSERT_TRUE(m_upTarget->attach_endpoint({ brypt::protocol::tcp, "lo", "127.0.0.1:35216" }));
    std::this_thread::sleep_for(1s); // Wait enough time for the services to spin up and connect. 

    // Verify configuration has been disabled after startup. 
    {
        EXPECT_FALSE(m_upService->set_option(brypt::option::base_path, "basepath"));
        EXPECT_FALSE(m_upService->set_option(brypt::option::configuration_filename, "filename"));
        EXPECT_FALSE(m_upService->set_option(brypt::option::bootstrap_filename, "filename"));
        EXPECT_FALSE(m_upService->set_option(brypt::option::core_threads, 1));
        EXPECT_FALSE(m_upService->set_option(brypt::option::identifier_type, brypt::identifier_type::ephemeral));
        EXPECT_FALSE(m_upService->set_option(brypt::option::use_bootstraps, false));
        EXPECT_FALSE(m_upService->set_option(brypt::option::node_name, "name"));
        EXPECT_FALSE(m_upService->set_option(brypt::option::node_description, "description"));
        EXPECT_FALSE(m_upService->set_option(brypt::option::security_strategy, brypt::security_strategy::pqnistl3));
        EXPECT_FALSE(m_upService->set_option(brypt::option::log_level, brypt::log_level::off));
    }

    // Verify event subscriptions have been disabled after startup. 
    {
        using enum local::EventObserver::Watching;
        auto& upObserver = m_observers.emplace_back(std::make_unique<local::EventObserver>(Service));
        EXPECT_FALSE(upObserver->Subscribe(*m_upService)); // Verify we can not subscribe to events after startup.
        // Unsubscribing is not implemented, so it's only safe to delete the observer if we correctly failed to subscribe.
        m_observers.pop_back(); 
    }

    std::this_thread::sleep_for(500ms);

    EXPECT_TRUE(m_upService->is_peer_connected(targetIdentifier));
    EXPECT_TRUE(m_upTarget->is_peer_connected(serviceIdentifier));
    
    // Verify message dispatching.
    {
        auto const result = m_upService->dispatch(targetIdentifier, "/ping", brypt::helpers::marshall("dispatch"));
        EXPECT_TRUE(result.is_success());
    }

    {
        auto const optDispatched = m_upService->cluster_dispatch("/ping", brypt::helpers::marshall("dispatch"));
        EXPECT_EQ(optDispatched, std::size_t{ 1 });
    }

    {
        auto const optDispatched = m_upService->sample_dispatch("/ping", brypt::helpers::marshall("dispatch"), 0.5);
        EXPECT_TRUE(optDispatched);
    }

    // Verify message requests. 
    {
        auto const result = m_upService->request(
            targetIdentifier, "/query", brypt::helpers::marshall("request"), 
            [&] (brypt::response const& response) {
                EXPECT_EQ(response.get_source(), targetIdentifier);
                EXPECT_EQ(brypt::helpers::to_string_view(response.get_payload()), "response");
            },
            [&] (brypt::response const&) { EXPECT_FALSE(true); });
        EXPECT_TRUE(result.is_success());
    }

    {
        auto const optRequested = m_upService->cluster_request(
            "/query", brypt::helpers::marshall("request"), 
            [&] (brypt::response const& response) {
                EXPECT_EQ(response.get_source(), targetIdentifier);
                EXPECT_EQ(brypt::helpers::to_string_view(response.get_payload()), "response");
            },
            [&] (brypt::response const&) { EXPECT_FALSE(true); });
        EXPECT_EQ(optRequested, std::size_t{ 1 });
    }

    {
        auto const optRequested = m_upService->sample_request(
            "/query", brypt::helpers::marshall("request"), 0.5, 
            [&] (brypt::response const& response) {
                EXPECT_EQ(response.get_source(), targetIdentifier);
                EXPECT_EQ(brypt::helpers::to_string_view(response.get_payload()), "response");
            },
            [&] (brypt::response const&) { EXPECT_FALSE(true); });
        EXPECT_TRUE(optRequested);
    }

    // Verify requests work more than once. 
    {
        auto const result = m_upService->request(
            targetIdentifier, "/query", brypt::helpers::marshall("request"), 
            [&] (brypt::response const& response) {
                EXPECT_EQ(response.get_source(), targetIdentifier);
                EXPECT_EQ(brypt::helpers::to_string_view(response.get_payload()), "response");
            },
            [&] (brypt::response const&) { EXPECT_FALSE(true); });
        EXPECT_TRUE(result.is_success());
    }

    // Verify requests to missing routes. 
    {
        auto const result = m_upService->request(
            targetIdentifier, "/missing", brypt::helpers::marshall("request"), 
            [] (brypt::response const&) { EXPECT_FALSE(true); },
            [&] (brypt::response const& response) {
                EXPECT_EQ(response.get_source(), targetIdentifier);
                EXPECT_TRUE(response.get_payload().empty());
                EXPECT_TRUE(response.get_status().has_error_code());
                EXPECT_EQ(response.get_status(), brypt::status_code::request_timeout);
            });
        EXPECT_TRUE(result.is_success());
    }

    // Verify requests to rejecting routes. 
    {
        auto const result = m_upService->request(
            targetIdentifier, "/rejecting", brypt::helpers::marshall("request"),
            [](brypt::response const&) { EXPECT_FALSE(true); },
            [&](brypt::response const& response) {
                EXPECT_EQ(response.get_source(), targetIdentifier);
                EXPECT_EQ(brypt::helpers::to_string_view(response.get_payload()), "rejected");
                EXPECT_TRUE(response.get_status().has_error_code());
                EXPECT_EQ(response.get_status(), brypt::status_code::bad_request);
            });
        EXPECT_TRUE(result.is_success());
    }

    std::this_thread::sleep_for(2s);

    // Verify the statistics from the service's point of view.
    {
        auto const optStatistics = m_upService->get_peer_statistics(targetIdentifier);
        ASSERT_TRUE(optStatistics);
        EXPECT_GE(optStatistics->get_sent(), std::uint32_t{ 8 });
        EXPECT_GE(optStatistics->get_received(), std::uint32_t{ 8 });
    }
    
    // Verify the details from the service's point of view.
    {
        auto const optDetails = m_upService->get_peer_details(targetIdentifier);
        ASSERT_TRUE(optDetails);
        EXPECT_EQ(optDetails->get_connection_state(), brypt::connection_state::connected);
        EXPECT_EQ(optDetails->get_authorization_state(), brypt::authorization_state::authorized);
        EXPECT_GE(optDetails->get_sent(), std::uint32_t{ 8 });
        EXPECT_GE(optDetails->get_received(), std::uint32_t{ 8 });

        auto const& remotes = optDetails->get_remotes();
        EXPECT_EQ(remotes.size(), std::size_t{ 1 });

        auto const itr = std::ranges::find_if(remotes, [] (auto const& remote) {
            return remote.is_bootstrapable();
        });
        ASSERT_NE(itr, remotes.end());
        EXPECT_EQ(itr->get_protocol(), brypt::protocol::tcp);
        EXPECT_EQ(itr->get_uri(), "tcp://127.0.0.1:35216");
    }

    std::this_thread::sleep_for(1s);

    ASSERT_TRUE(m_upService->disconnect(m_upTarget->get_identifier())); // Verify we can disconnect by identifier. 
    std::this_thread::sleep_for(500ms);

    ASSERT_TRUE(m_upService->connect(brypt::protocol::tcp, "127.0.0.1:35216")); // Verify we can reconnect. 
    std::this_thread::sleep_for(500ms);

    ASSERT_TRUE(m_upService->disconnect(brypt::protocol::tcp, "127.0.0.1:35216")); // Verify we can disconnect by address. 
    std::this_thread::sleep_for(500ms);

    ASSERT_TRUE(m_upService->connect(brypt::protocol::tcp, "127.0.0.1:35216")); // Verify we can reconnect.
    std::this_thread::sleep_for(500ms);

    // Verify we can detach an endpoint while running. 
    ASSERT_TRUE(m_upTarget->detach_endpoint(brypt::protocol::tcp, "127.0.0.1:35216"));
    std::this_thread::sleep_for(500ms); // Wait enough time for the endpoint to shutdown and for any unpublished events.

    EXPECT_TRUE(m_upTarget->shutdown());
    EXPECT_TRUE(m_upService->shutdown());

    // Verify the details from the service's point of view.
    {
        auto const optDetails = m_upService->get_peer_details(targetIdentifier);
        ASSERT_TRUE(optDetails);
        EXPECT_EQ(optDetails->get_connection_state(), brypt::connection_state::disconnected);
        EXPECT_EQ(optDetails->get_authorization_state(), brypt::authorization_state::unauthorized);
        EXPECT_GT(optDetails->get_sent(), std::uint32_t{ 8 });
        EXPECT_GT(optDetails->get_received(), std::uint32_t{ 8 });
        EXPECT_TRUE(optDetails->get_remotes().empty());
    }

    EXPECT_EQ(m_observers.size(), 3); // We should have two observers on the main service and one on the target. 
    for(auto const& upObserver : m_observers) {
        EXPECT_TRUE(upObserver->IsSequenceExpected()); // Both observers should have received the correct events.
    }
}

//----------------------------------------------------------------------------------------------------------------------

local::EventObserver::EventObserver(Watching watching)
    : m_watching(watching)
    , m_record()
{
}

//----------------------------------------------------------------------------------------------------------------------

bool local::EventObserver::Subscribe(brypt::service& service)
{
    std::vector<brypt::result> results;
    {
        brypt::result const result = service.subscribe<brypt::event::binding_failed>
            ([this] (auto protocol, auto uri, auto const& result) {
                // This test observer expects to be provided a valid protocol, a non-empty uri, and an error code. 
                bool const expected = protocol == brypt::protocol::tcp && !uri.empty() && result.is_error();
                m_record.emplace_back(brypt::event::binding_failed, expected);
            });
            
        results.emplace_back(result);
    }

    {
        brypt::result const result = service.subscribe<brypt::event::connection_failed>(
            [this] (auto protocol, auto uri, auto const& result) {
                // This test observer expects to be provided a valid protocol, a non-empty uri, and an error code. 
                bool const expected = protocol == brypt::protocol::tcp && !uri.empty() && result.is_error();
                m_record.emplace_back(brypt::event::connection_failed, expected);
            });

        results.emplace_back(result);
    }

    {
        brypt::result const result = service.subscribe<brypt::event::endpoint_started>(
            [this] (auto protocol, auto uri) {
                // This test observer expects to be provided a valid protocol and a non-empty uri.
                bool const expected = protocol == brypt::protocol::tcp && !uri.empty();
                m_record.emplace_back(brypt::event::endpoint_started, expected);
            });

        results.emplace_back(result);
    }

    {
        brypt::result const result = service.subscribe<brypt::event::endpoint_stopped>(
            [this] (auto protocol, auto uri, auto const& result) {
                // This test observer expects to be provided a valid protocol, a non-empty uri, and a success code.
                bool expected;
                expected = protocol == brypt::protocol::tcp;
                expected = expected && !uri.empty();
                expected = expected && result == brypt::result_code::shutdown_requested;
                m_record.emplace_back(brypt::event::endpoint_stopped, expected);
            });

        results.emplace_back(result);
    }

    {
        brypt::result const result = service.subscribe<brypt::event::peer_connected>(
            [this] (auto identifier, auto protocol) {
                // This test observer expects to be provided a non-empty identifier and a valid protocol.
                bool const expected = !identifier.empty() && protocol == brypt::protocol::tcp;
                m_record.emplace_back(brypt::event::peer_connected, expected);
            });

        results.emplace_back(result);
    }

    {
        brypt::result const result = service.subscribe<brypt::event::peer_disconnected>(
            [this] (auto identifier, auto protocol, auto const& result) {
                // This test observer expects to be provided a non-empty identifier and a cancellation result code.
                using enum brypt::result_code;
                bool const expected = 
                    !identifier.empty() && protocol == brypt::protocol::tcp && 
                    ( result == shutdown_requested || result == session_closed);
                m_record.emplace_back(brypt::event::peer_disconnected, expected);
            });

        results.emplace_back(result);
    }

    {
        brypt::result const result = service.subscribe<brypt::event::runtime_started>([this] () {
            // This handler is always expected.
            m_record.emplace_back(brypt::event::runtime_started, true);
        });

        results.emplace_back(result);
    }

    {
        brypt::result const result = service.subscribe<brypt::event::runtime_stopped>([this] (auto const& result) {
            // This test observer expects the runtime stopped to always indicate a success.
            m_record.emplace_back(brypt::event::runtime_stopped, result.is_success());
        });
        
        results.emplace_back(result);
    }

    bool const subscribed = std::ranges::all_of(results, [] (brypt::result const& result) {
        return result.is_success();
    });

    return subscribed;
}

//----------------------------------------------------------------------------------------------------------------------

bool local::EventObserver::IsSequenceExpected() const
{
    switch (m_watching) {
        case Watching::Service: return IsServiceSequenceExpected();
        case Watching::Target: return IsTargetSequenceExpected();
    }

    return false;
}

//----------------------------------------------------------------------------------------------------------------------

bool local::EventObserver::IsServiceSequenceExpected() const
{
    if (m_record.size() != 10) { return false; }
    if (m_record[0] != EventEntry{ brypt::event::runtime_started, true }) { return false; }
    if (m_record[1] != EventEntry{ brypt::event::endpoint_started, true }) { return false; }
    if (m_record[2] != EventEntry{ brypt::event::peer_connected, true }) { return false; }
    if (m_record[3] != EventEntry{ brypt::event::peer_disconnected, true }) { return false; }
    if (m_record[4] != EventEntry{ brypt::event::peer_connected, true }) { return false; }
    if (m_record[5] != EventEntry{ brypt::event::peer_disconnected, true }) { return false; }
    if (m_record[6] != EventEntry{ brypt::event::peer_connected, true }) { return false; }
    if (m_record[7] != EventEntry{ brypt::event::peer_disconnected, true }) { return false; }
    if (m_record[8] != EventEntry{ brypt::event::endpoint_stopped, true }) { return false; }
    if (m_record[9] != EventEntry{ brypt::event::runtime_stopped, true }) { return false; }
    return true;
}

//----------------------------------------------------------------------------------------------------------------------

bool local::EventObserver::IsTargetSequenceExpected() const
{
    if (m_record.size() != 9) { return false; }
    if (m_record[0] != EventEntry{ brypt::event::runtime_started, true }) { return false; }
    if (m_record[1] != EventEntry{ brypt::event::endpoint_started, true }) { return false; }
    if (m_record[2] != EventEntry{ brypt::event::peer_connected, true }) { return false; }
    if (m_record[3] != EventEntry{ brypt::event::peer_disconnected, true }) { return false; }
    if (m_record[4] != EventEntry{ brypt::event::peer_connected, true }) { return false; }
    if (m_record[5] != EventEntry{ brypt::event::peer_disconnected, true }) { return false; }
    if (m_record[6] != EventEntry{ brypt::event::peer_connected, true }) { return false; }
    if (m_record[7] != EventEntry{ brypt::event::endpoint_stopped, true }) { return false; }
    if (m_record[8] != EventEntry{ brypt::event::runtime_stopped, true }) { return false; }
    return true;
}

//----------------------------------------------------------------------------------------------------------------------
