//------------------------------------------------------------------------------------------------
#pragma once
//------------------------------------------------------------------------------------------------
#include "../Utilities/NetworkUtils.hpp"
#include "../Utilities/NodeUtils.hpp"
//------------------------------------------------------------------------------------------------
#include <string>
#include <shared_mutex>
//------------------------------------------------------------------------------------------------

class CCoordinatorState {
public:
    CCoordinatorState();
    CCoordinatorState(
        NodeUtils::TechnologyType technology,
        NetworkUtils::AddressComponentPair const& entryComponents);
    
    NodeUtils::NodeIdType GetId() const;
    std::string GetEntry() const;
    NodeUtils::TechnologyType GetTechnology() const;

    void SetId(NodeUtils::NodeIdType const& id);
    void SetEntry(NetworkUtils::AddressComponentPair const& id);
    void SetTechnology(NodeUtils::TechnologyType technology);

private:
    mutable std::shared_mutex m_mutex;

    NodeUtils::NodeIdType m_id;    // Coordinator identification number of the node's coordinator
    std::string m_entry; // The combination of the two provided entry components 
    NodeUtils::TechnologyType m_technology;
};

//------------------------------------------------------------------------------------------------