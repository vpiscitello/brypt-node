//------------------------------------------------------------------------------------------------
#pragma once
//------------------------------------------------------------------------------------------------
#include "BryptIdentifier/BryptIdentifier.hpp"
#include "Utilities/NetworkUtils.hpp"
#include "Components/Network/Protocol.hpp"
//------------------------------------------------------------------------------------------------
#include <string>
#include <shared_mutex>
//------------------------------------------------------------------------------------------------

class CoordinatorState {
public:
    CoordinatorState();

    BryptIdentifier::SharedContainer GetBryptIdentifier() const;
    void SetBryptIdentifier(BryptIdentifier::SharedContainer const& spBryptIdentifier);

private:
    mutable std::shared_mutex m_mutex;
    BryptIdentifier::SharedContainer m_spBryptIdentifier; // BryptIdentifier of the node's coordinator
};

//------------------------------------------------------------------------------------------------