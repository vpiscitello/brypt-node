//----------------------------------------------------------------------------------------------------------------------
// File: Path.hpp
// Description: 
//----------------------------------------------------------------------------------------------------------------------
#pragma once
//----------------------------------------------------------------------------------------------------------------------
#include <ranges>
#include <string>
#include <string_view>
#include <vector>
#include <stdexcept>
//----------------------------------------------------------------------------------------------------------------------

//----------------------------------------------------------------------------------------------------------------------
namespace Route {
//----------------------------------------------------------------------------------------------------------------------

class Path;

//----------------------------------------------------------------------------------------------------------------------
} // Route namespace
//----------------------------------------------------------------------------------------------------------------------

class Route::Path
{
public: 
    static constexpr std::string_view Separator = "/";

    Path() = default;
    explicit Path(std::string_view path);

    [[nodiscard]] std::string const& GetRoot() const;
    [[nodiscard]] std::string_view GetParent() const;
    [[nodiscard]] std::string const& GetTail() const;

    [[nodiscard]] auto const GetRange() const;
    [[nodiscard]] auto const GetParentRange() const;

    [[nodiscard]] Path CloneRoot() const;
    [[nodiscard]] Path CloneParent() const;

    [[nodiscard]] bool IsValid() const;
    [[nodiscard]] std::size_t GetComponentsSize() const;

    [[nodiscard]] std::string ToString() const;

    [[nodiscard]] bool Replace(std::string&& path);
    [[nodiscard]] bool Append(std::string&& component);
    [[nodiscard]] bool SetTail(std::string&& component);

private:
    template<typename Taken>
    explicit Path(std::ranges::take_view<Taken> const& components);

    void Build(std::string_view path);

    static constexpr std::string_view Empty = "";
    std::vector<std::string> m_components;
};

//----------------------------------------------------------------------------------------------------------------------

template<typename Taken>
Route::Path::Path(std::ranges::take_view<Taken> const& components)
    : m_components(components.begin(), components.end())
{
}

//----------------------------------------------------------------------------------------------------------------------

inline auto const Route::Path::GetRange() const { return m_components | std::views::all; }

//----------------------------------------------------------------------------------------------------------------------

inline auto const Route::Path::GetParentRange() const
{
    if (m_components.empty()) { throw std::runtime_error("Unable to fetch details on an invalid path!"); }
    return m_components | std::views::take(m_components.size() - 1);
}

//----------------------------------------------------------------------------------------------------------------------
