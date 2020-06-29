//------------------------------------------------------------------------------------------------
// File: IEndpointMediator.hpp
// Description: 
//------------------------------------------------------------------------------------------------
#pragma once
//------------------------------------------------------------------------------------------------
#include "../Components/Endpoints/TechnologyType.hpp"
//------------------------------------------------------------------------------------------------
#include <string>
#include <unordered_map>
//------------------------------------------------------------------------------------------------

class IEndpointMediator
{
public:
    using EndpointEntries = std::unordered_map<Endpoints::TechnologyType, std::string>;

    virtual ~IEndpointMediator() = default;
    virtual EndpointEntries GetEndpointEntries() const = 0;
};

//------------------------------------------------------------------------------------------------