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

template<brypt::suported_primitive_type ValueType>
bool VerifyValueResult(std::optional<ValueType> const& optValue, auto const& input);
bool VerifyValueResult(std::optional<std::reference_wrapper<std::string const>> const& optValue, auto const& input);

//----------------------------------------------------------------------------------------------------------------------
} // local namespace
//----------------------------------------------------------------------------------------------------------------------
namespace test {
//----------------------------------------------------------------------------------------------------------------------

using OptionInputs = boost::hana::tuple<bool, std::int32_t, std::string>;
using OptionNames = std::vector<brypt_option_t>;

//----------------------------------------------------------------------------------------------------------------------
} // local namespace
} // namespace
//----------------------------------------------------------------------------------------------------------------------

TEST(BryptLibrarySuite, OptionTest)
{
    test::OptionInputs const inputs = { true, 35216, "brypt" };

    test::OptionNames const names = {
        brypt::option::use_bootstraps, 
        brypt::option::base_path,
        brypt::option::configuration_filename, 
        brypt::option::peers_filename,
    };

    auto VerifyOptionConversion = [this] <brypt::supported_option_type ValueType> (
        brypt::option const& option, auto const& input, bool isExpectedInput)
    {
        EXPECT_EQ(option.is<ValueType>(), isExpectedInput);

        ValueType value;
        auto const status = option.get(value);
        EXPECT_EQ(status.is_nominal(), isExpectedInput);
        if (status.is_nominal()) { EXPECT_EQ(value, std::any_cast<ValueType const&>(input)); }

        auto const optValue = option.value<ValueType>();
        EXPECT_EQ(optValue != std::nullopt, isExpectedInput);

        EXPECT_TRUE(local::VerifyValueResult<ValueType>(optValue, input));
    };

    for (auto const& name : names) {
        boost::hana::for_each(inputs, [this, &name, &VerifyOptionConversion] (auto const& input) {
            bool const isExpectedInput = local::IsExpectedInputType(name, input);
            
            brypt::status status;
            brypt::option option(name, input, status);
            
            EXPECT_EQ(status.is_nominal(), isExpectedInput);
            EXPECT_EQ(option.has_value(), isExpectedInput);

            VerifyOptionConversion.template operator()<std::decay_t<decltype(input)>>(option, input, isExpectedInput);
        });
    }
}

//----------------------------------------------------------------------------------------------------------------------

bool local::IsExpectedInputType(brypt_option_t option, auto const& input)
{
    switch (option) {
        case brypt::option::use_bootstraps: {
            return typeid(input) == typeid(bool);
        }
        case brypt::option::base_path: 
        case brypt::option::configuration_filename: 
        case brypt::option::peers_filename: {
            return typeid(input) == typeid(std::string);
        }
        default: assert(false); return false;
    }
}

//----------------------------------------------------------------------------------------------------------------------

template<brypt::suported_primitive_type ValueType> 
bool local::VerifyValueResult(std::optional<ValueType> const& optValue, auto const& input)
{
    if (optValue) { return *optValue == input; }
    return true;
}

//----------------------------------------------------------------------------------------------------------------------

bool local::VerifyValueResult(
    std::optional<std::reference_wrapper<std::string const>> const& optValue, auto const& input)
{
    if (optValue) { return optValue.value().get() == input; }
    return true;
}

//----------------------------------------------------------------------------------------------------------------------
