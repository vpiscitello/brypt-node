//----------------------------------------------------------------------------------------------------------------------
// File: EndpointMediator.hpp
// Description: 
//----------------------------------------------------------------------------------------------------------------------
#pragma once
//----------------------------------------------------------------------------------------------------------------------
#include "Components/Network/EndpointIdentifier.hpp"
//----------------------------------------------------------------------------------------------------------------------

namespace Network { class Address; class BindingAddress; }

//----------------------------------------------------------------------------------------------------------------------

class IEndpointMediator
{
public:
    virtual ~IEndpointMediator() = default;

    [[nodiscard]] virtual bool IsAddressRegistered(Network::Address const& address) const = 0;
    virtual void UpdateBinding(Network::Endpoint::Identifier identifier, Network::BindingAddress const& binding) = 0;
};

//----------------------------------------------------------------------------------------------------------------------