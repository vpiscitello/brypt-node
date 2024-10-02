//----------------------------------------------------------------------------------------------------------------------
#include "Components/Route/Path.hpp"
//----------------------------------------------------------------------------------------------------------------------
#include <gtest/gtest.h>
//----------------------------------------------------------------------------------------------------------------------
#include <algorithm>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>
//----------------------------------------------------------------------------------------------------------------------

//----------------------------------------------------------------------------------------------------------------------
namespace {
namespace local {
//----------------------------------------------------------------------------------------------------------------------

//----------------------------------------------------------------------------------------------------------------------
} // local namespace
//----------------------------------------------------------------------------------------------------------------------
namespace test {
//----------------------------------------------------------------------------------------------------------------------

using PathExpectations = std::vector<std::tuple<
    std::string, std::vector<std::string>, std::string, std::string, std::string, bool>>;

//----------------------------------------------------------------------------------------------------------------------
} // local namespace
} // namespace
//----------------------------------------------------------------------------------------------------------------------

TEST(RoutePathSuite, DefaultConstructorTest)
{
    Route::Path path;

    EXPECT_ANY_THROW([[maybe_unused]] auto const test = path.GetRoot());
    EXPECT_ANY_THROW([[maybe_unused]] auto const test = path.GetParent());
    EXPECT_ANY_THROW([[maybe_unused]] auto const test = path.GetTail()); 
    EXPECT_TRUE(std::ranges::equal(path.GetRange(), std::vector<std::string>{}));
    EXPECT_ANY_THROW([[maybe_unused]] auto const test = path.GetParentRange());
    EXPECT_ANY_THROW([[maybe_unused]] auto const test = path.CloneRoot());
    EXPECT_ANY_THROW([[maybe_unused]] auto const test = path.CloneParent());

    EXPECT_EQ(path.ToString(), "");
    EXPECT_FALSE(path.IsValid());
    EXPECT_EQ(path.GetComponentsSize(), 0);

    EXPECT_FALSE(path.Replace("\\query"));
    EXPECT_FALSE(path.IsValid());

    EXPECT_TRUE(path.Replace("/query"));
    EXPECT_TRUE(path.IsValid());
    EXPECT_TRUE(std::ranges::equal(path.GetRange(), std::vector<std::string>{ "query" }));
    EXPECT_EQ(path.ToString(), "/query");

    EXPECT_TRUE(path.Append("data"));
    EXPECT_TRUE(path.IsValid());
    EXPECT_TRUE(std::ranges::equal(path.GetRange(), std::vector<std::string>{ "query", "data" }));
    EXPECT_EQ(path.ToString(), "/query/data");

    EXPECT_TRUE(path.SetTail("information"));
    EXPECT_TRUE(path.IsValid());
    EXPECT_TRUE(std::ranges::equal(path.GetRange(), std::vector<std::string>{ "query", "information" }));
    EXPECT_EQ(path.ToString(), "/query/information");
}

//----------------------------------------------------------------------------------------------------------------------

TEST(RoutePathSuite, StringConstructorTest)
{
    test::PathExpectations const expectations = {
        { "/query", { "query" }, "query", "", "query", true },
        { "/query/data", { "query", "data" }, "query", "query", "data", true },
        { "/query/data/", { "query", "data" }, "query", "query", "data", true },
        { "/query/data/temperature", { "query", "data", "temperature" }, "query", "data", "temperature", true },
        { "/query/data/temperature/", { "query", "data", "temperature" }, "query", "data", "temperature", true },
        { "/query/1", { "query", "1" }, "query", "query", "1", true },
        { "/1", { "1" }, "1", "", "1", true },
        { "/1/2/3/", { "1", "2", "3" }, "1", "2", "3", true },
        { "/a/b/c/d", { "a", "b", "c", "d" }, "a", "c", "d", true },
        { 
            "/abcdefghijklmnopqrstuvwxyz/ABCDEFGHIJKLMNOPQRSTUVWXYZ/0123456789",
            { "abcdefghijklmnopqrstuvwxyz", "ABCDEFGHIJKLMNOPQRSTUVWXYZ", "0123456789" },
            "abcdefghijklmnopqrstuvwxyz", "ABCDEFGHIJKLMNOPQRSTUVWXYZ", "0123456789", true
        },
        { "", { }, "", "", "", false },
        { "/", { }, "", "", "", false },
        { "//", { }, "", "", "", false },
        { "///", { }, "", "", "", false },
        { "/#", { }, "", "", "", false },
        { "/.", { }, "", "", "", false },
        { "query", { }, "", "", "", false },
        { "que\0ry", { }, "", "", "", false },
        { "query\n", { }, "", "", "", false },
        { "\\query", { }, "", "", "", false },
        { "\\query\\data", { }, "", "", "", false },
        { "\\query\\data\\", { }, "", "", "", false },
        { "\\query/data\\", { }, "", "", "", false },
        { "\t/query", { }, "", "", "", false },
        { "/query?", { }, "", "", "", false },
        { "/query&data", { }, "", "", "", false },
        { "/query/data//", { }, "", "", "", false },
        { "/que_ry/data", { }, "", "", "", false },
        { "/query/_", { }, "", "", "", false },
        { "/query/_/data", { }, "", "", "", false },
        { "/query//data", { }, "", "", "", false },
        { "\"/query\"", { }, "", "", "", false },
    };

    for (auto const& [input, components, root, parent, segment, valid] : expectations) {
        Route::Path path{ input };

        EXPECT_EQ(path.IsValid(), valid);
        EXPECT_EQ(path.GetComponentsSize(), components.size());
        EXPECT_TRUE(std::ranges::equal(path.GetRange(), components));

        // The behavior of the path differs dependeing on its validity. When the path is invalid most methods
        // will throw to indicate an error. 
        if (path.IsValid()) {
            EXPECT_EQ(path.GetTail(), segment);
            EXPECT_EQ(path.GetRoot(), root);
            EXPECT_EQ(path.GetParent(), parent);
            
            // If the provided input has a trailing '/', it will be dropped when the string is recreated.
            auto const expected = (input.size() > 0 && input.back() == '/') ? 
                input | std::views::take(input.size() - 1) : input | std::views::take(input.size());
            EXPECT_TRUE(std::ranges::equal(path.ToString(), expected));

            EXPECT_TRUE(std::ranges::equal(
                path.GetParentRange(), 
                components | std::views::take_while([&segment] (auto const& str) { return str != segment; })));

            EXPECT_TRUE(std::ranges::equal(path.CloneRoot().GetRange(), std::vector<std::string>{ root } ));

            EXPECT_TRUE(std::ranges::equal(
                path.CloneParent().GetRange(), 
                components | std::views::take_while([&segment] (auto const& str) { return str != segment; })));
        } else {
            EXPECT_ANY_THROW([[maybe_unused]] auto const test = path.GetTail()); 
            EXPECT_ANY_THROW([[maybe_unused]] auto const test = path.GetRoot());
            EXPECT_ANY_THROW([[maybe_unused]] auto const test = path.GetParent());
            EXPECT_ANY_THROW([[maybe_unused]] auto const test = path.CloneRoot());
            EXPECT_ANY_THROW([[maybe_unused]] auto const test = path.GetParentRange());
            EXPECT_ANY_THROW([[maybe_unused]] auto const test = path.CloneParent());
            EXPECT_TRUE(path.ToString().empty());
        }

        // Regradless of the initial validation status, adding a component will result in a valid path different 
        // from the initial input. 
        EXPECT_TRUE(path.SetTail("replaced"));
        EXPECT_TRUE(path.IsValid());
        EXPECT_NE(path.ToString(), input);
    }
}

//----------------------------------------------------------------------------------------------------------------------
