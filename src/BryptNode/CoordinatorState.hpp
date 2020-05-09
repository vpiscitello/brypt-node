//------------------------------------------------------------------------------------------------
#pragma once
//------------------------------------------------------------------------------------------------
#include "../Utilities/NetworkUtils.hpp"
#include "../Utilities/NodeUtils.hpp"
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
    
    NodeUtils::NodeIdType GetId() const;
    std::string GetEntry() const;
    Endpoints::TechnologyType GetTechnology() const;

    void SetId(NodeUtils::NodeIdType const& id);
    void SetEntry(NetworkUtils::AddressComponentPair const& id);
    void SetTechnology(Endpoints::TechnologyType technology);

private:
    mutable std::shared_mutex m_mutex;

    NodeUtils::NodeIdType m_id;    // Coordinator identification number of the node's coordinator
    std::string m_entry; // The combination of the two provided entry components 
    Endpoints::TechnologyType m_technology;
};

//------------------------------------------------------------------------------------------------