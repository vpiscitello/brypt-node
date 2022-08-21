//----------------------------------------------------------------------------------------------------------------------
// File: Router.cpp
// Description: 
//----------------------------------------------------------------------------------------------------------------------
#include "Router.hpp"
#include "BryptNode/ServiceProvider.hpp"
#include "Components/Peer/Proxy.hpp"
#include "Utilities/Logger.hpp"
//----------------------------------------------------------------------------------------------------------------------
#include <ranges>
//----------------------------------------------------------------------------------------------------------------------

//----------------------------------------------------------------------------------------------------------------------
namespace {
namespace local {
//----------------------------------------------------------------------------------------------------------------------

//----------------------------------------------------------------------------------------------------------------------
} // local namespace
} // namespace
//----------------------------------------------------------------------------------------------------------------------

Route::Router::Router()
    : m_logger(spdlog::get(Logger::Name::Core.data()))
    , m_root(Path::Seperator)
{
}

//----------------------------------------------------------------------------------------------------------------------

bool Route::Router::Contains(std::string_view route) const
{
    return Match(route) != nullptr;
}

//----------------------------------------------------------------------------------------------------------------------

bool Route::Router::Route(Message::Application::Parcel const& message, Peer::Action::Next& next) const
{
    constexpr std::string_view UnrecognizedRoute = 
        "Failed to match a message handler to an unrecognized route [\"{}\"] received from {}";
    constexpr std::string_view HandlerWarning = 
        "Route [\"{}\"] failed to handle a message received from {}";
    constexpr std::string_view ExceptionError =
        "Route [\"{}\"] encountered an exception handling a message received from {}: \"{}\"";

    try {
        if (auto const* const pMatchedNode = Match(message.GetRoute()); pMatchedNode) {
            bool const success = pMatchedNode->OnMessage(message, next);
            if (!success) { m_logger->warn(HandlerWarning, message.GetRoute(), message.GetSource()); }
            return success;
        }

        m_logger->warn(UnrecognizedRoute, message.GetRoute(), message.GetSource());
    } catch (std::exception const& e) {
        m_logger->error(ExceptionError, message.GetRoute(), message.GetSource(), e.what());
    }

    return false;
}

//----------------------------------------------------------------------------------------------------------------------

Route::Router::Prefix* Route::Router::Register(std::string_view route)
{
    assert(Assertions::Threading::IsCoreThread());

    // If the provided route is not valid, then it cannot be registered. 
    if (auto const path = Path{ route }; !path.IsValid()) { return nullptr; }

    // If the route ends with a seperator, it is implicity removed (i.e. "/route/" is just "/route").
    if (route.ends_with(Path::Seperator)) { route.remove_suffix(1); }

    // The router is implemented as a radix trie. This will determine where a new prefix node 
    // should be inserted. Existing node's may be split and/or children re-ordered to keep the nodes sorted. 
    auto pCurrent = &m_root;
    while (true) {
        // Find the longest common prefix between the given prefix and current prefix. 
        constexpr auto FindCommonPrefix = [] (std::string_view const& left, std::string_view const& right) {
            auto const result = std::ranges::mismatch(left, right);
            std::size_t const size = std::ranges::distance(left.begin(), result.in1);
            return std::string_view{ left.data(), size };
        };

        auto const& prefix = pCurrent->GetPrefix();
        auto const common = FindCommonPrefix(route, prefix);

        // If the common prefix is shorter than the current node's prefix. A branch node needs to be created
        // for the prefix split before inserting the new node.
        if (common.size() < prefix.size()) { pCurrent->Split(common.size()); }

        // If the common prefix is the provided route, then replace the node's handler. 
        if (common.size() == route.size()) { return pCurrent; }

        // If the common prefix is shorter than the prefix to be inserted than a new child node needs to created. 
        route.remove_prefix(common.size()); // Remove the prefix that already exists in the trie. 

        // If no node could be found for the route's starting character, insert the new node as a child of the current.
        auto const [found, result] = pCurrent->BinaryFind(route.front());
        if (!found) { return pCurrent->Insert(route, result); }
        pCurrent = &*result;
    }

    return nullptr;
}

//----------------------------------------------------------------------------------------------------------------------

bool Route::Router::Initialize(std::shared_ptr<Node::ServiceProvider> const& spServiceProvider)
{
    assert(Assertions::Threading::IsCoreThread());
    return m_root.Initialize(spServiceProvider);
}

//----------------------------------------------------------------------------------------------------------------------

Route::Router::Prefix* Route::Router::Match(std::string_view route)
{
    return const_cast<Prefix*>(std::as_const(*this).Match(route));
}

//----------------------------------------------------------------------------------------------------------------------

Route::Router::Prefix const* Route::Router::Match(std::string_view route) const
{
    assert(Assertions::Threading::IsCoreThread());

    if (route.empty()) [[unlikely]] { return nullptr; }

    // If the route ends with a seperator, it is implicity removed (i.e. "/route/" is just "/route").
    if (route.ends_with(Path::Seperator)) { route.remove_suffix(1); }
    
    auto pCurrent = &m_root;
    while (true) {
        auto const& prefix = pCurrent->GetPrefix();

        // If the route is now the same size as the current prefix, we must check to see if the current
        // prefix node refers to a valid message and the two terms are equal. 
        if (route.size() == prefix.size()) { 
            return (pCurrent->ReferencesHandler() && route == prefix) ? pCurrent : nullptr;
        }

        // If the route size is now less than the current prefix, there is no way a match can be found.  
        if (route.size() < prefix.size()) { return nullptr; }

        // We must validate the route matches the current prefix up to the prefix boundary. 
        if (!std::ranges::equal(route | std::views::take(prefix.size()), prefix)) { return nullptr; }

        route.remove_prefix(prefix.size()); // Remove the characters of the route that have already been validated.

        auto const [found, result] =  pCurrent->BinaryFind(route.front());
        if (!found) { return nullptr; }
        pCurrent = &*result;
    }

    return nullptr;
}

//----------------------------------------------------------------------------------------------------------------------

Route::Router::Prefix::Prefix(std::string_view prefix)
    : m_prefix(prefix)
    , m_children()
    , m_upMessageHandler()
{
}

//----------------------------------------------------------------------------------------------------------------------

Route::Router::Prefix::Prefix(std::string_view prefix, Prefix& other)
    : m_prefix(prefix)
    , m_children()
    , m_upMessageHandler(std::move(other.m_upMessageHandler))
{
    m_children.swap(other.m_children);
}

//----------------------------------------------------------------------------------------------------------------------

char const& Route::Router::Prefix::GetFront() const { return m_prefix.front(); }

//----------------------------------------------------------------------------------------------------------------------

std::string const& Route::Router::Prefix::GetPrefix() const { return m_prefix; }

//----------------------------------------------------------------------------------------------------------------------

Route::Router::Prefix::Children& Route::Router::Prefix::GetChildren() { return m_children; }

//----------------------------------------------------------------------------------------------------------------------

Route::Router::Prefix::Children const& Route::Router::Prefix::GetChildren() const { return m_children; }

//----------------------------------------------------------------------------------------------------------------------

bool Route::Router::Prefix::ReferencesHandler() const { return m_upMessageHandler != nullptr; }

//----------------------------------------------------------------------------------------------------------------------

void Route::Router::Prefix::Split(std::size_t boundary)
{
    // Create the new child to represent the prefix after the new boundary. The new child will take ownership of the
    // current node's resources (i.e. lookup, children, and handler).
    Prefix child{ m_prefix.substr(boundary), *this };
    m_children.emplace_back(std::move(child));
    m_prefix.resize(boundary); // Clear the characters after the new boundary. 
}

//----------------------------------------------------------------------------------------------------------------------

std::pair<bool, Route::Router::Prefix::Children::iterator> Route::Router::Prefix::BinaryFind(char const& value)
{
    auto const result = std::ranges::lower_bound(
        m_children, value, std::less{}, [] (auto const& prefix) { return prefix.GetFront(); });
    return result != m_children.end() && !(value < result->GetFront()) ?
        std::make_pair(true, result) : std::make_pair(false, result);
}

//----------------------------------------------------------------------------------------------------------------------

std::pair<bool, Route::Router::Prefix::Children::const_iterator> Route::Router::Prefix::BinaryFind(char const& value) const
{
    auto const result = std::ranges::lower_bound(
        m_children, value, std::less{}, [] (auto const& prefix) { return prefix.GetFront(); });
    return result != m_children.end() && !(value < result->GetFront()) ?
        std::make_pair(true, result) : std::make_pair(false, result);
}

//----------------------------------------------------------------------------------------------------------------------

Route::Router::Prefix* Route::Router::Prefix::Insert(std::string_view route, Children::iterator const& hint)
{
    auto const result = m_children.emplace(hint, route);
    return &*result;
}

//----------------------------------------------------------------------------------------------------------------------

Route::Router::Prefix::AttachResult Route::Router::Prefix::Attach(std::unique_ptr<IMessageHandler>&& upMessageHandler)
{
    bool const empty = m_upMessageHandler == nullptr;
    m_upMessageHandler = std::move(upMessageHandler);
    return (empty) ? AttachResult::Success : AttachResult::Replaced; 
}

//----------------------------------------------------------------------------------------------------------------------

bool Route::Router::Prefix::Initialize(std::shared_ptr<Node::ServiceProvider> const& spServiceProvider)
{
    bool success = (m_upMessageHandler) ? m_upMessageHandler->OnFetchServices(spServiceProvider) : true;
    return success && std::ranges::all_of(m_children, [&spServiceProvider] (auto& child) {
        return child.Initialize(spServiceProvider);
    });
}

//----------------------------------------------------------------------------------------------------------------------

bool Route::Router::Prefix::OnMessage(Message::Application::Parcel const& message, Peer::Action::Next& next) const
{
    if (!m_upMessageHandler) { return false; }
    return m_upMessageHandler->OnMessage(message, next);
}

//----------------------------------------------------------------------------------------------------------------------
