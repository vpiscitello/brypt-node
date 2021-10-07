
//----------------------------------------------------------------------------------------------------------------------
// File: Actions.hpp
// Description: 
//----------------------------------------------------------------------------------------------------------------------
#pragma once
//----------------------------------------------------------------------------------------------------------------------
#include "BryptIdentifier/IdentifierTypes.hpp"
#include "BryptMessage/ShareablePack.hpp"
//----------------------------------------------------------------------------------------------------------------------
#include <functional>
//----------------------------------------------------------------------------------------------------------------------

namespace Network {
    class RemoteAddress; 

    using MessageVariant = std::variant<std::string, Message::ShareablePack>;
    using MessageAction = std::function<bool(Node::Identifier const& destination, MessageVariant&& message)>;
    using DisconnectAction = std::function<bool(RemoteAddress const& destination)>;
}

//----------------------------------------------------------------------------------------------------------------------
