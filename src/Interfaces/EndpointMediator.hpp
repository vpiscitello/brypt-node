//----------------------------------------------------------------------------------------------------------------------
// File: IEndpointMediator.hpp
// Description: 
//----------------------------------------------------------------------------------------------------------------------
#pragma once
//----------------------------------------------------------------------------------------------------------------------
#include "Components/Network/Protocol.hpp"
//----------------------------------------------------------------------------------------------------------------------
#include <string>
#include <set>
#include <unordered_map>
//----------------------------------------------------------------------------------------------------------------------

class IEndpointMediator
{
public:
    using EndpointEntryMap = std::unordered_map<Network::Protocol, std::string>;
    using EndpointUriSet = std::set<std::string>;

    virtual ~IEndpointMediator() = default;
    [[nodiscard]] virtual EndpointEntryMap GetEndpointEntries() const = 0;
    [[nodiscard]] virtual EndpointUriSet GetEndpointUris() const = 0;
};

//----------------------------------------------------------------------------------------------------------------------