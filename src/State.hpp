//------------------------------------------------------------------------------------------------
// File: State
// Description:
//------------------------------------------------------------------------------------------------
#pragma once
//------------------------------------------------------------------------------------------------
#include "Utilities/NodeUtils.hpp"
//------------------------------------------------------------------------------------------------
#include <ctime>
#include <memory>
#include <mutex>
#include <shared_mutex>
#include <string>
#include <set>
//------------------------------------------------------------------------------------------------

class CState;

//------------------------------------------------------------------------------------------------
namespace State {
//------------------------------------------------------------------------------------------------

class CAuthority;
class CCoordinator;
class CNetwork;
class CSecurity;
class CSelf;
class CSensor;

//------------------------------------------------------------------------------------------------
} // State namespace
//------------------------------------------------------------------------------------------------

class State::CAuthority {
public:
    CAuthority()
        : m_address(NodeUtils::AUTHORITY_ADDRESS)
        , m_token(std::string())
        , m_authorityStateMutex()
    {
    };

    NodeUtils::IPv4Address GetAddress() const 
    {
        std::shared_lock<std::shared_mutex> lock(m_authorityStateMutex);
        return m_address;
    };

    std::string GetToken() const
    {
        std::shared_lock<std::shared_mutex> lock(m_authorityStateMutex);
        return m_token;
    };

    void SetAddress(NodeUtils::IPv4Address const& address)
    {
        std::unique_lock<std::shared_mutex> lock(m_authorityStateMutex);
        m_address = address;
    };

    void SetToken(std::string const& token)
    {
        std::unique_lock<std::shared_mutex> lock(m_authorityStateMutex);
        m_token = token;
    };

private:
    NodeUtils::IPv4Address m_address;  // Networking address of the central authority for the Brypt ecosystem
    std::string m_token; // Access token for the Brypt network

    mutable std::shared_mutex m_authorityStateMutex;
};

//------------------------------------------------------------------------------------------------

class State::CCoordinator {
public:
    CCoordinator()
        : m_id(std::string())
        , m_address(std::string())
        , m_requestPort(std::string())
        , m_publisherPort(std::string())
        , m_technology(NodeUtils::TechnologyType::NONE)
        , m_coordinatorStateMutex()
    {
    };

    CCoordinator(
        NodeUtils::NodeIdType const& id,
        NodeUtils::IPv4Address const& address,
        NodeUtils::PortNumber const& port,
        NodeUtils::TechnologyType const& technology)
        : m_id(id)
        , m_address(address)
        , m_requestPort(port)
        , m_publisherPort(std::to_string(std::stoi(port) + 1))
        , m_technology(technology)
        , m_coordinatorStateMutex()
    {
    };
    
    NodeUtils::NodeIdType GetId() const 
    {
        std::shared_lock<std::shared_mutex> lock(m_coordinatorStateMutex);
        return m_id;
    };

    NodeUtils::IPv4Address GetAddress() const
    {
        std::shared_lock<std::shared_mutex> lock(m_coordinatorStateMutex);
        return m_address;
    };

    NodeUtils::PortNumber GetRequestPort() const
    {
        std::shared_lock<std::shared_mutex> lock(m_coordinatorStateMutex);
        return m_requestPort;
    };

    NodeUtils::PortNumber GetPublisherPort() const
    {
        std::shared_lock<std::shared_mutex> lock(m_coordinatorStateMutex);
        return m_publisherPort;
    };

    NodeUtils::TechnologyType GetTechnology() const
    {
        std::shared_lock<std::shared_mutex> lock(m_coordinatorStateMutex);
        return m_technology;
    };


    void SetId(NodeUtils::NodeIdType const& id)
    {
        std::unique_lock<std::shared_mutex> lock(m_coordinatorStateMutex);
        m_id = id;
    };

    void SetAddress(NodeUtils::IPv4Address const& address)
    {
        std::unique_lock<std::shared_mutex> lock(m_coordinatorStateMutex);
        m_address = address;
    };

    void SetRequestPort(NodeUtils::PortNumber const& port)
    {
        std::unique_lock<std::shared_mutex> lock(m_coordinatorStateMutex);
        m_requestPort = port;
    };

    void SetPublisherPort(NodeUtils::PortNumber const& port)
    {
        std::unique_lock<std::shared_mutex> lock(m_coordinatorStateMutex);
        m_publisherPort = port;
    };

    void SetTechnology(NodeUtils::TechnologyType const& technology)
    {
        std::unique_lock<std::shared_mutex> lock(m_coordinatorStateMutex);
        m_technology = technology;
    };

private:
    NodeUtils::NodeIdType m_id;    // Coordinator identification number of the node's coordinator
    NodeUtils::IPv4Address m_address;
    NodeUtils::PortNumber m_requestPort;
    NodeUtils::PortNumber m_publisherPort;
    NodeUtils::TechnologyType m_technology;

    mutable std::shared_mutex m_coordinatorStateMutex;
};

//------------------------------------------------------------------------------------------------

class State::CNetwork {
public:
    CNetwork()
        : m_peerNames()
        , m_uptime()
        , m_registered()
        , m_updated()
        , m_networkStateMutex()
    {
    };

    std::set<NodeUtils::NodeIdType> GetPeerNames() const 
    {
        std::shared_lock<std::shared_mutex> lock(m_networkStateMutex);
        return m_peerNames;
    };

    std::size_t GetKnownNodes() const
    {
        std::shared_lock<std::shared_mutex> lock(m_networkStateMutex);
        return m_peerNames.size();
    };

    NodeUtils::TimePeriod GetUptimeCount() const
    {
        std::shared_lock<std::shared_mutex> lock(m_networkStateMutex);
        return m_uptime;
    };

    NodeUtils::TimePoint GetRegisteredTimePoint() const
    {
        std::shared_lock<std::shared_mutex> lock(m_networkStateMutex);
        return m_registered;
    }

    NodeUtils::TimePoint GetUpdatedTimePoint() const
    {
        std::shared_lock<std::shared_mutex> lock(m_networkStateMutex);
        return m_updated;
    }

    void PushPeerName(NodeUtils::NodeIdType const& peerName)
    {
        std::unique_lock<std::shared_mutex> lock(m_networkStateMutex);
        if (auto const itr = m_peerNames.find(peerName); itr != m_peerNames.end()) {
            m_peerNames.emplace(peerName);
            m_updated = NodeUtils::GetSystemTimePoint();
        }
    }

    void RemovePeerName(NodeUtils::NodeIdType const& peerName)
    {
        std::unique_lock<std::shared_mutex> lock(m_networkStateMutex);
        if (auto const itr = m_peerNames.find(peerName); itr != m_peerNames.end()) {
            m_peerNames.erase(itr);
            m_updated = NodeUtils::GetSystemTimePoint();
        } 
    }

    void SetRegisteredTimePoint(NodeUtils::TimePoint const& timepoint)
    {
        std::unique_lock<std::shared_mutex> lock(m_networkStateMutex);
        m_registered = timepoint;
    };

    void Updated()
    {
        std::unique_lock<std::shared_mutex> lock(m_networkStateMutex);
        m_updated = NodeUtils::GetSystemTimePoint();
    };

private:
    std::set<NodeUtils::NodeIdType> m_peerNames;

    NodeUtils::TimePeriod m_uptime; // The amount of time the node has been live
    NodeUtils::TimePoint m_registered; // The timestamp the node was added to the network
    NodeUtils::TimePoint m_updated;    // The timestamp the node was last updated

    mutable std::shared_mutex m_networkStateMutex;
};

//------------------------------------------------------------------------------------------------

class State::CSecurity {
public:
    CSecurity()
        : m_protocol(std::string())
        , m_securityStateMutex()
    {
    };

    explicit CSecurity(std::string const& protocol)
        : m_protocol(protocol)
        , m_securityStateMutex()
    {
    };

    std::string GetProtocol() const
    {
        std::shared_lock<std::shared_mutex> lock(m_securityStateMutex);
        return m_protocol;
    };

    void SetProtocol(std::string const& protocol)
    {
        std::unique_lock<std::shared_mutex> lock(m_securityStateMutex);
        m_protocol = protocol;
    }

private:
    std::string m_protocol;

    mutable std::shared_mutex m_securityStateMutex;
};

//------------------------------------------------------------------------------------------------

class State::CSelf {
public:
    CSelf()
        : m_id(std::string())
        , m_serial(std::string())
        , m_address(NodeUtils::GetLocalAddress())
        , m_port(std::string())
        , m_nextAvailablePort(0)
        , m_cluster(std::string())
        , m_operation(NodeUtils::DeviceOperation::NONE)
        , m_technologies()
        , m_selfStateMutex()
    {
    };

    CSelf(
        NodeUtils::NodeIdType const& id,
        NodeUtils::PortNumber const& port,
        NodeUtils::DeviceOperation const& operation,
        std::set<NodeUtils::TechnologyType> const& technologies)
        : m_id(id)
        , m_serial(std::string())
        , m_address(NodeUtils::GetLocalAddress())
        , m_port(port)
        , m_publisherPort(std::to_string(std::stoi(port) + 1))
        , m_nextAvailablePort(std::stoi(port) + NodeUtils::PORT_GAP)
        , m_cluster(std::string())
        , m_operation(operation)
        , m_technologies(technologies)
        , m_selfStateMutex()
    {
    };

    NodeUtils::NodeIdType GetId() const
    {
        std::shared_lock<std::shared_mutex> lock(m_selfStateMutex);
        return m_id;
    };

    NodeUtils::SerialNumber GetSerial() const
    {
        std::shared_lock<std::shared_mutex> lock(m_selfStateMutex);
        return m_serial;
    };

    NodeUtils::IPv4Address GetAddress() const
    {
        std::shared_lock<std::shared_mutex> lock(m_selfStateMutex);
        return m_address;
    };

    NodeUtils::PortNumber GetPort() const
    {
        std::shared_lock<std::shared_mutex> lock(m_selfStateMutex);
        return m_port;
    };

    NodeUtils::PortNumber GetPublisherPort() const
    {
        std::shared_lock<std::shared_mutex> lock(m_selfStateMutex);
        return m_publisherPort;
    };

    std::uint32_t GetNextPort()
    {
        std::shared_lock<std::shared_mutex> lock(m_selfStateMutex);
        return ++m_nextAvailablePort; // TODO: Need smarter method of choosing the next port
    };

    NodeUtils::ClusterIdType GetCluster() const
    {
        std::shared_lock<std::shared_mutex> lock(m_selfStateMutex);
        return m_cluster;
    };

    NodeUtils::DeviceOperation GetOperation() const
    {
        std::shared_lock<std::shared_mutex> lock(m_selfStateMutex);
        return m_operation;
    };

    std::set<NodeUtils::TechnologyType> GetTechnologies() const
    {
        std::shared_lock<std::shared_mutex> lock(m_selfStateMutex);
        return m_technologies;
    };

    void SetId(NodeUtils::NodeIdType const& id)
    {
        std::unique_lock<std::shared_mutex> lock(m_selfStateMutex);
        m_id = id;
    }

    void SetSerial(NodeUtils::SerialNumber const& serial)
    {
        std::unique_lock<std::shared_mutex> lock(m_selfStateMutex);
        m_serial = serial;
    }

    void SetCluster(NodeUtils::ClusterIdType const& cluster)
    {
        std::unique_lock<std::shared_mutex> lock(m_selfStateMutex);
        m_cluster = cluster;
    }

    void SetOperation(NodeUtils::DeviceOperation const& operation)
    {
        std::unique_lock<std::shared_mutex> lock(m_selfStateMutex);
        m_operation = operation;
    }

    void SetTechnologies(std::set<NodeUtils::TechnologyType> const& technologies)
    {
        std::unique_lock<std::shared_mutex> lock(m_selfStateMutex);
        m_technologies = technologies;
    }

private:
    NodeUtils::NodeIdType m_id;// Network identification number of the node
    NodeUtils::SerialNumber m_serial; // Hardset identification number of the device
    NodeUtils::IPv4Address m_address;  // IP address of the node
    NodeUtils::PortNumber m_port;  // Main request port of the node
    NodeUtils::PortNumber m_publisherPort; // Port for the node publishing socket
    std::uint32_t m_nextAvailablePort;
    NodeUtils::ClusterIdType m_cluster;   // Cluster identification number of the node's cluster
    NodeUtils::DeviceOperation m_operation;  // A boolean value of the node's root status
    std::set<NodeUtils::TechnologyType> m_technologies; // Communication technologies of the node

    mutable std::shared_mutex m_selfStateMutex;
};

//------------------------------------------------------------------------------------------------

class State::CSensor {
public:
    CSensor()
        : m_pin()
        , m_sensorStateMutex()
    {
    };

    std::uint8_t GetPin() const
    {
        std::shared_lock<std::shared_mutex> lock(m_sensorStateMutex);
        return m_pin;
    };

    void SetPin(std::uint8_t pin)
    {
        std::unique_lock<std::shared_mutex> lock(m_sensorStateMutex);
        m_pin = pin;
    }

private:
    std::uint8_t m_pin;   // The GPIO pin the node will read from

    mutable std::shared_mutex m_sensorStateMutex;
};

//------------------------------------------------------------------------------------------------

class CState {
public:
    explicit CState(NodeUtils::TOptions const& options)
        : m_authority(std::make_shared<State::CAuthority>())
        , m_coordinator(std::make_shared<State::CCoordinator>(
            options.m_peerName, 
            options.m_peerAddress,
            options.m_peerPort,
            NodeUtils::TechnologyType::DIRECT))
        , m_network(std::make_shared<State::CNetwork>())
        , m_security(std::make_shared<State::CSecurity>(std::string(NodeUtils::ENCRYPTION_PROTOCOL)))
        , m_self(std::make_shared<State::CSelf>(
            options.m_id,
            options.m_port,
            options.m_operation,
            std::set<NodeUtils::TechnologyType>{options.m_technology}
        ))
        , m_sensor(std::make_shared<State::CSensor>())
    {
        m_network->PushPeerName(options.m_peerName);
    };

    std::weak_ptr<State::CAuthority> const& GetAuthorityState() const
    {
        return m_authority;
    };

    std::weak_ptr<State::CCoordinator> const& GetCoordinatorState() const
    {
        return m_coordinator;
    };

    std::weak_ptr<State::CNetwork> const& GetNetworkState() const
    {
        return m_network;
    };

    std::weak_ptr<State::CSecurity> const& GetSecurityState() const
    {
        return m_security;
    };

    std::weak_ptr<State::CSelf> const& GetSelfState() const
    {
        return m_self;
    };

    std::weak_ptr<State::CSensor> const& GetSensorState() const
    {
        return m_sensor;
    };

private:
    std::shared_ptr<State::CAuthority> m_authority;
    std::shared_ptr<State::CCoordinator> m_coordinator;
    std::shared_ptr<State::CNetwork> m_network;
    std::shared_ptr<State::CSecurity> m_security;
    std::shared_ptr<State::CSelf> m_self;
    std::shared_ptr<State::CSensor> m_sensor;
};

//------------------------------------------------------------------------------------------------
