//------------------------------------------------------------------------------------------------
#pragma once
//------------------------------------------------------------------------------------------------
#include "../Utilities/NodeUtils.hpp"
#include <string>
#include <shared_mutex>
//------------------------------------------------------------------------------------------------

class CCoordinatorState {
public:
    CCoordinatorState();
    CCoordinatorState(
        NodeUtils::TechnologyType technology,
        NodeUtils::AddressComponentPair entryComponents);
    
    NodeUtils::NodeIdType GetId() const;
    NodeUtils::NetworkAddress GetAddress() const;
    std::string GetRequestEntry() const;
    NodeUtils::PortNumber GetRequestPort() const;
    std::string GetPublisherEntry() const;
    NodeUtils::PortNumber GetPublisherPort() const;
    NodeUtils::TechnologyType GetTechnology() const;

    void SetId(NodeUtils::NodeIdType const& id);
    void SetAddress(NodeUtils::NetworkAddress const& address);
    void SetRequestPort(NodeUtils::PortNumber const& port);
    void SetPublisherPort(NodeUtils::PortNumber const& port);
    void SetTechnology(NodeUtils::TechnologyType technology);

private:
    mutable std::shared_mutex m_mutex;

    NodeUtils::NodeIdType m_id;    // Coordinator identification number of the node's coordinator
    
    NodeUtils::NetworkAddress m_address;
    NodeUtils::PortNumber m_requestPort;
    NodeUtils::PortNumber m_publisherPort;

    std::string m_requestEntry; // The combination of the address and request port 
    std::string m_publisherEntry; // The combination of the address and publisher port

    NodeUtils::TechnologyType m_technology;
};

//------------------------------------------------------------------------------------------------