//------------------------------------------------------------------------------------------------
// File: IEndpointMediator.hpp
// Description: 
//------------------------------------------------------------------------------------------------
#pragma once
//------------------------------------------------------------------------------------------------
#include "Components/Network/Protocol.hpp"
//------------------------------------------------------------------------------------------------
#include <string>
#include <set>
#include <unordered_map>
//------------------------------------------------------------------------------------------------

class IEndpointMediator
{
public:
    using EndpointEntryMap = std::unordered_map<Endpoints::TechnologyType, std::string>;
    using EndpointURISet = std::set<std::string>;

    virtual ~IEndpointMediator() = default;
    virtual EndpointEntryMap GetEndpointEntries() const = 0;
    virtual EndpointURISet GetEndpointURIs() const = 0;
};

//------------------------------------------------------------------------------------------------