//----------------------------------------------------------------------------------------------------------------------
#include <brypt/brypt.hpp>
//----------------------------------------------------------------------------------------------------------------------
#include <gtest/gtest.h>
//----------------------------------------------------------------------------------------------------------------------
#include <boost/hana.hpp>
#include <boost/hana/ext/std/tuple.hpp>
//----------------------------------------------------------------------------------------------------------------------
#include <any>
#include <typeindex>
#include <type_traits>
#include <vector>
//----------------------------------------------------------------------------------------------------------------------

//----------------------------------------------------------------------------------------------------------------------
namespace {
namespace local {
//----------------------------------------------------------------------------------------------------------------------

bool IsExpectedInputType(brypt_option_t option, auto const& input);

//----------------------------------------------------------------------------------------------------------------------
} // local namespace
//----------------------------------------------------------------------------------------------------------------------
namespace test {
//----------------------------------------------------------------------------------------------------------------------

using OptionInputs = boost::hana::tuple<bool, std::int32_t, brypt::log_level, std::string>;
using OptionNames = std::vector<brypt_option_t>;

//----------------------------------------------------------------------------------------------------------------------
} // local namespace
} // namespace
//----------------------------------------------------------------------------------------------------------------------

//----------------------------------------------------------------------------------------------------------------------
TEST(LibraryOptionSuite, ValueTest)
{
    test::OptionInputs const inputs = { true, 35216, brypt::log_level::info, "brypt" };

    test::OptionNames const names = {
        brypt::option::core_threads,
        brypt::option::use_bootstraps, 
        brypt::option::log_level,
        brypt::option::base_path,
        brypt::option::configuration_filename, 
        brypt::option::bootstrap_filename,
    };

    auto VerifyOptionConversion = [this] <brypt::supported_option_type ValueType> (
        brypt::option& option, auto const& input, bool isExpectedInput)
    {
        EXPECT_EQ(option.contains<ValueType>(), isExpectedInput);

        ValueType value;
        auto const result = option.get(value);
        EXPECT_EQ(result.is_success(), isExpectedInput);
        if (result.is_success()) {
            EXPECT_EQ(value, std::any_cast<ValueType const&>(input));
            EXPECT_EQ(option.value<ValueType>(), std::any_cast<ValueType const&>(input));
            auto const moved = option.extract<ValueType>();
            EXPECT_EQ(moved, std::any_cast<ValueType const&>(input));

        } else {
            EXPECT_ANY_THROW([[maybe_unused]] auto const throwing = option.value<ValueType>());
        }
    };

    for (auto const& name : names) {
        boost::hana::for_each(inputs, [this, &name, &VerifyOptionConversion] (auto const& input) {
            bool const isExpectedInput = local::IsExpectedInputType(name, input);
            
            brypt::result result;
            brypt::option option(name, input, result);
            
            EXPECT_EQ(result.is_success(), isExpectedInput);
            EXPECT_EQ(option.has_value(), isExpectedInput);

            VerifyOptionConversion.template operator()<std::decay_t<decltype(input)>>(option, input, isExpectedInput);
        });
    }
}

//----------------------------------------------------------------------------------------------------------------------

bool local::IsExpectedInputType(brypt_option_t option, auto const& input)
{
    switch (option) {
        case brypt::option::use_bootstraps: return typeid(input) == typeid(bool);
        case brypt::option::core_threads: return typeid(input) == typeid(std::int32_t);
        case brypt::option::log_level: return typeid(input) == typeid(brypt::log_level);
        case brypt::option::base_path: 
        case brypt::option::configuration_filename: 
        case brypt::option::bootstrap_filename: return typeid(input) == typeid(std::string);
        default: assert(false); return false;
    }
}

//----------------------------------------------------------------------------------------------------------------------
