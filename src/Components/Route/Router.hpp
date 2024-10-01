//----------------------------------------------------------------------------------------------------------------------
// File: Router.hpp
// Description: 
//----------------------------------------------------------------------------------------------------------------------
#pragma once
//----------------------------------------------------------------------------------------------------------------------
#include "MessageHandler.hpp"
#include "Path.hpp"
#include "Components/Message/ApplicationMessage.hpp"
#include "Utilities/Assertions.hpp"
//----------------------------------------------------------------------------------------------------------------------
#include <spdlog/spdlog.h>
//----------------------------------------------------------------------------------------------------------------------
#include <cassert>
#include <concepts>
#include <cstdint>
#include <iterator>
#include <memory>
#include <string>
#include <string_view>
#include <vector>
//----------------------------------------------------------------------------------------------------------------------

namespace Node { class ServiceProvider; }

namespace Peer { 
    class Proxy;
    namespace Action { class Next; }
}

//----------------------------------------------------------------------------------------------------------------------
namespace Route {
//----------------------------------------------------------------------------------------------------------------------

class Router;
class IMessageHandler;

//----------------------------------------------------------------------------------------------------------------------
} // Route namespace
//----------------------------------------------------------------------------------------------------------------------

class Route::Router
{
public:
    Router();

    template<typename HandlerType, typename... Arguments>
	    requires std::derived_from<HandlerType, IMessageHandler>
    [[nodiscard]] bool Register(std::string_view route, Arguments&&... arguments)
    {
        if (auto const pNode = Register(route); pNode) {
            auto const result = pNode->Attach(std::make_unique<HandlerType>(std::forward<Arguments>(arguments)...));
            if (result == Prefix::AttachResult::Replaced) {
                m_logger->warn("The route handler for \"{}\" was replaced.", route);
            }
            return true;
        }

        return false;
    }

    [[nodiscard]] bool Initialize(std::shared_ptr<Node::ServiceProvider> const& spServiceProvider);

    [[nodiscard]] bool Contains(std::string_view route) const;
    [[nodiscard]] bool Route(Message::Application::Parcel const& message, Peer::Action::Next& next) const;

private:
    class Prefix
    {
    public:
        using Children = std::vector<Prefix>;
        enum class AttachResult : std::uint32_t { Success, Replaced };
        
        explicit Prefix(std::string_view prefix);
        Prefix(std::string_view prefix, Prefix& other);

        [[nodiscard]] char const& GetFront() const;
        [[nodiscard]] std::string const& GetPrefix() const;
        [[nodiscard]] Children& GetChildren();
        [[nodiscard]] Children const& GetChildren() const;
        [[nodiscard]] bool ReferencesHandler() const;

        void Split(std::size_t boundary);
        std::pair<bool, Children::iterator> BinaryFind(char const& value);
        std::pair<bool, Children::const_iterator> BinaryFind(char const& value) const;
        [[nodiscard]] Prefix* Insert(std::string_view route, Children::iterator const& hint);

        [[nodiscard]] AttachResult Attach(std::unique_ptr<IMessageHandler>&& upMessageHandler);
        [[nodiscard]] bool Initialize(std::shared_ptr<Node::ServiceProvider> const& spServiceProvider);
        [[nodiscard]] bool OnMessage(Message::Application::Parcel const& message, Peer::Action::Next& next) const;

    private: 
        std::string m_prefix;
        Children m_children;
        std::unique_ptr<IMessageHandler> m_upMessageHandler;
    };

    [[nodiscard]] Prefix* Register(std::string_view route);

    [[nodiscard]] Prefix* Match(std::string_view route);
    [[nodiscard]] Prefix const* Match(std::string_view route) const;

    std::shared_ptr<spdlog::logger> m_logger;
    Prefix m_root;
};

//----------------------------------------------------------------------------------------------------------------------
