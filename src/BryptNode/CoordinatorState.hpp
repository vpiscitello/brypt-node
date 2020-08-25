//------------------------------------------------------------------------------------------------
#pragma once
//------------------------------------------------------------------------------------------------
#include "../BryptIdentifier/BryptIdentifier.hpp"
#include "../Utilities/NetworkUtils.hpp"
#include "../Components/Endpoints/TechnologyType.hpp"
//------------------------------------------------------------------------------------------------
#include <string>
#include <shared_mutex>
//------------------------------------------------------------------------------------------------

class CCoordinatorState {
public:
    CCoordinatorState();
    CCoordinatorState(
        Endpoints::TechnologyType technology,
        NetworkUtils::AddressComponentPair const& entryComponents);
    
    BryptIdentifier::CContainer GetIdentifier() const;
    std::string GetEntry() const;
    Endpoints::TechnologyType GetTechnology() const;

    void SetIdentifier(BryptIdentifier::CContainer const& identifier);
    void SetEntry(NetworkUtils::AddressComponentPair const& id);
    void SetTechnology(Endpoints::TechnologyType technology);

private:
    mutable std::shared_mutex m_mutex;

    BryptIdentifier::CContainer m_identifier; // BryptIdentifier of the node's coordinator
    std::string m_entry; // The combination of the two provided entry components 
    Endpoints::TechnologyType m_technology;
};

//------------------------------------------------------------------------------------------------