//----------------------------------------------------------------------------------------------------------------------
#pragma once
//----------------------------------------------------------------------------------------------------------------------
#include "BryptIdentifier/BryptIdentifier.hpp"
//----------------------------------------------------------------------------------------------------------------------
#include <string>
#include <shared_mutex>
//----------------------------------------------------------------------------------------------------------------------

class CoordinatorState {
public:
    CoordinatorState();

    Node::SharedIdentifier GetNodeIdentifier() const;
    void SetNodeIdentifier(Node::SharedIdentifier const& spNodeIdentifier);

private:
    mutable std::shared_mutex m_mutex;
    Node::SharedIdentifier m_spNodeIdentifier; // The identifier of the node's coordinator
};

//----------------------------------------------------------------------------------------------------------------------