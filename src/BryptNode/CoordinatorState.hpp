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

    BryptIdentifier::SharedContainer GetBryptIdentifier() const;
    void SetBryptIdentifier(BryptIdentifier::SharedContainer const& spBryptIdentifier);

private:
    mutable std::shared_mutex m_mutex;
    BryptIdentifier::SharedContainer m_spBryptIdentifier; // BryptIdentifier of the node's coordinator

};

//------------------------------------------------------------------------------------------------