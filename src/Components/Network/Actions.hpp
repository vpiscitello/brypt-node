
//----------------------------------------------------------------------------------------------------------------------
// File: Actions.hpp
// Description: 
//----------------------------------------------------------------------------------------------------------------------
#pragma once
//----------------------------------------------------------------------------------------------------------------------
#include "Components/Identifier/IdentifierTypes.hpp"
#include "Components/Message/ShareablePack.hpp"
//----------------------------------------------------------------------------------------------------------------------
#include <functional>
#include <variant>
//----------------------------------------------------------------------------------------------------------------------

namespace Network {
    class RemoteAddress; 

    using MessageVariant = std::variant<std::string, Message::ShareablePack>;
    using MessageAction = std::function<bool(Node::Identifier const& destination, MessageVariant&& message)>;
    using DisconnectAction = std::function<bool(RemoteAddress const& destination)>;
}

//----------------------------------------------------------------------------------------------------------------------
