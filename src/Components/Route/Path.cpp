//----------------------------------------------------------------------------------------------------------------------
// File: Path.hpp
// Description: 
//----------------------------------------------------------------------------------------------------------------------
#include "Path.hpp"
//----------------------------------------------------------------------------------------------------------------------
#include <algorithm>
#include <cassert>
#include <cctype>
#include <cstdint>
#include <sstream>
//----------------------------------------------------------------------------------------------------------------------

Route::Path::Path(std::string_view const& path)
    : m_components()
{
    Build(path);
}

//----------------------------------------------------------------------------------------------------------------------

std::string const& Route::Path::GetRoot() const
{
    if (m_components.empty()) { throw std::runtime_error("Unable to fetch details on an invalid path!"); }
    return m_components.front();
}

//----------------------------------------------------------------------------------------------------------------------

std::string_view Route::Path::GetParent() const
{
    if (m_components.empty()) { throw std::runtime_error("Unable to fetch details on an invalid path!"); }
    return (m_components.size() > 1) ? m_components.rbegin()[1] : Empty;
}

//----------------------------------------------------------------------------------------------------------------------

std::string const& Route::Path::GetTail() const
{
    if (m_components.empty()) { throw std::runtime_error("Unable to fetch details on an invalid path!"); }
    return m_components.back();
}

//----------------------------------------------------------------------------------------------------------------------

Route::Path Route::Path::CloneRoot() const
{
    if (m_components.empty()) { throw std::runtime_error("Unable to fetch details on an invalid path!"); }
    return Path{ m_components | std::views::take(1) };
}

//----------------------------------------------------------------------------------------------------------------------

Route::Path Route::Path::CloneParent() const
{
    if (m_components.empty()) { throw std::runtime_error("Unable to fetch details on an invalid path!"); }
    return Path{ m_components | std::views::take(m_components.size() - 1) };
}

//----------------------------------------------------------------------------------------------------------------------

bool Route::Path::IsValid() const { return !m_components.empty(); }

//----------------------------------------------------------------------------------------------------------------------

std::size_t Route::Path::GetComponentsSize() const { return m_components.size(); }

//----------------------------------------------------------------------------------------------------------------------

std::string Route::Path::ToString() const
{
    std::ostringstream path;
    std::ranges::for_each(m_components, [&path] (auto const& str) { path << Seperator << str; });
    return path.str();
}

//----------------------------------------------------------------------------------------------------------------------

bool Route::Path::Replace(std::string&& path)
{
    Build(path);
    return IsValid();
}

//----------------------------------------------------------------------------------------------------------------------

bool Route::Path::Append(std::string&& component)
{
    if (std::ranges::any_of(component, [] (auto character) { return !std::isalnum(character); })) { return false; }
    m_components.emplace_back(std::move(component));
    return true;
}

//----------------------------------------------------------------------------------------------------------------------

bool Route::Path::SetTail(std::string&& component)
{
    if (std::ranges::any_of(component, [] (auto character) { return !std::isalnum(character); })) { return false; }
    auto& replaced = (m_components.empty()) ? m_components.emplace_back() : m_components.back();
    replaced = std::move(component);
    return true;
}

//----------------------------------------------------------------------------------------------------------------------

void Route::Path::Build(std::string_view const& path)
{
    if (!path.starts_with(Seperator)) { return; }

    bool error = false;
    std::ranges::transform(
        path | std::views::drop(Seperator.size()) | std::views::split(Seperator), std::back_inserter(m_components), 
        [&error] (auto&& partition) {
            std::string component;
            auto const size = static_cast<std::size_t>(std::ranges::distance(partition));
            component.reserve(size);
            std::ranges::copy_if(partition, std::back_inserter(component), [] (auto&& character) { 
                return std::isalnum(character);
            });
            error = error || component.empty() || component.size() != size;
            return component;
        });

    if (error) { m_components.clear(); }
}

//----------------------------------------------------------------------------------------------------------------------
