//----------------------------------------------------------------------------------------------------------------------
// File: AssociatedMessage.hpp
// Description:
//----------------------------------------------------------------------------------------------------------------------
#pragma once
//----------------------------------------------------------------------------------------------------------------------
#include "BryptMessage/ApplicationMessage.hpp"
//----------------------------------------------------------------------------------------------------------------------
#include <utility>
#include <memory>
//----------------------------------------------------------------------------------------------------------------------

namespace Peer { class Proxy; }

//----------------------------------------------------------------------------------------------------------------------

using AssociatedMessage = std::pair<std::weak_ptr<Peer::Proxy>, Message::Application::Parcel>;

//----------------------------------------------------------------------------------------------------------------------
