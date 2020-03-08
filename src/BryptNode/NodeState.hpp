//------------------------------------------------------------------------------------------------
#pragma once
//------------------------------------------------------------------------------------------------
#include "../Utilities/NodeUtils.hpp"
#include <string>
#include <cstdint>
#include <set>
#include <shared_mutex>
//------------------------------------------------------------------------------------------------

class CNodeState {
public:
    CNodeState();

    CNodeState(
        std::string_view const& interface,
        NodeUtils::AddressComponentPair const& bindingComponents,
        NodeUtils::DeviceOperation operation,
        std::set<NodeUtils::TechnologyType> const& technologies);

    std::uint32_t GetNextPort();

    NodeUtils::NodeIdType GetId() const;
    NodeUtils::SerialNumber GetSerial() const;
    std::string GetBinding() const;
    NodeUtils::NetworkAddress GetAddress() const;
    NodeUtils::PortNumber GetPort() const;
    NodeUtils::PortNumber GetPublisherPort() const;
    NodeUtils::ClusterIdType GetCluster() const;
    NodeUtils::DeviceOperation GetOperation() const;
    std::set<NodeUtils::TechnologyType> GetTechnologies() const;

    void SetId(NodeUtils::NodeIdType const& id);
    void SetSerial(NodeUtils::SerialNumber const& serial);
    void SetCluster(NodeUtils::ClusterIdType const& cluster);
    void SetOperation(NodeUtils::DeviceOperation operation);
    void SetTechnologies(std::set<NodeUtils::TechnologyType> const& technologies);

private:
    mutable std::shared_mutex m_mutex;

    NodeUtils::NodeIdType m_id;// Network identification number of the node
    NodeUtils::SerialNumber m_serial; // Hardset identification number of the device

    NodeUtils::NetworkAddress m_address;  // IP address of the node
    NodeUtils::PortNumber m_port;  // Main request port of the node
    std::string m_binding; // The string combination of address and port
    NodeUtils::PortNumber m_publisherPort; // Port for the node publishing socket
    std::uint32_t m_nextAvailablePort;
 
    NodeUtils::ClusterIdType m_cluster;   // Cluster identification number of the node's cluster
    NodeUtils::DeviceOperation m_operation;  // A boolean value of the node's root status
    std::set<NodeUtils::TechnologyType> m_technologies; // Communication technologies of the node
};

//------------------------------------------------------------------------------------------------