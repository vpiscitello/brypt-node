//------------------------------------------------------------------------------------------------
// File: State
// Description:
//------------------------------------------------------------------------------------------------
#pragma once
//------------------------------------------------------------------------------------------------
#include "Configuration/Configuration.hpp"
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
    explicit CAuthority(std::string_view url)
        : m_url(url)
        , m_token(std::string())
        , m_mutex()
    {
    };

    std::string GetUrl() const 
    {
        std::shared_lock<std::shared_mutex> lock(m_mutex);
        return m_url;
    };

    std::string GetToken() const
    {
        std::shared_lock<std::shared_mutex> lock(m_mutex);
        return m_token;
    };

    void SetAddress(std::string_view url)
    {
        std::unique_lock<std::shared_mutex> lock(m_mutex);
        m_url = url;
    };

    void SetToken(std::string const& token)
    {
        std::unique_lock<std::shared_mutex> lock(m_mutex);
        m_token = token;
    };

private:
    std::string m_url;  // Networking url of the central authority for the Brypt ecosystem
    std::string m_token; // Access token for the Brypt network

    mutable std::shared_mutex m_mutex;
};

//------------------------------------------------------------------------------------------------

class State::CCoordinator {
public:
    CCoordinator()
        : m_id(0)
        , m_address(std::string())
        , m_requestPort(std::string())
        , m_publisherPort(std::string())
        , m_technology(NodeUtils::TechnologyType::None)
        , m_mutex()
    {
    };

    CCoordinator(
        NodeUtils::TechnologyType technology,
        NodeUtils::AddressComponentPair entryComponents)
        : m_id(0)
        , m_address(entryComponents.first)
        , m_requestPort(entryComponents.second)
        , m_publisherPort(std::string())
        , m_requestEntry(std::string())
        , m_publisherEntry(std::string())
        , m_technology(technology)
        , m_mutex()
    {
        // If the address or port is not set on the entry components it implies 
        // that a coordinator has not been set in the configuration and thus there
        // is no initial state to parse. 
        if (m_address.empty() || m_requestPort.empty()) {
            return;
        }

        std::int32_t const port = std::stoi(m_requestPort);
        m_publisherPort = std::to_string(port + 1);

        m_requestEntry = m_address + NodeUtils::ADDRESS_COMPONENT_SEPERATOR + m_requestPort;
        m_publisherEntry = m_address + NodeUtils::ADDRESS_COMPONENT_SEPERATOR + m_publisherPort;
    };
    
    NodeUtils::NodeIdType GetId() const 
    {
        std::shared_lock<std::shared_mutex> lock(m_mutex);
        return m_id;
    };

    NodeUtils::NetworkAddress GetAddress() const
    {
        std::shared_lock<std::shared_mutex> lock(m_mutex);
        return m_address;
    };

    std::string GetRequestEntry() const
    {
        std::shared_lock<std::shared_mutex> lock(m_mutex);
        return m_requestEntry;
    }

    NodeUtils::PortNumber GetRequestPort() const
    {
        std::shared_lock<std::shared_mutex> lock(m_mutex);
        return m_requestPort;
    };

    std::string GetPublisherEntry() const
    {
        std::shared_lock<std::shared_mutex> lock(m_mutex);
        return m_publisherEntry;
    }

    NodeUtils::PortNumber GetPublisherPort() const
    {
        std::shared_lock<std::shared_mutex> lock(m_mutex);
        return m_publisherPort;
    };

    NodeUtils::TechnologyType GetTechnology() const
    {
        std::shared_lock<std::shared_mutex> lock(m_mutex);
        return m_technology;
    };

    void SetId(NodeUtils::NodeIdType const& id)
    {
        std::unique_lock<std::shared_mutex> lock(m_mutex);
        m_id = id;
    };

    void SetAddress(NodeUtils::NetworkAddress const& address)
    {
        std::unique_lock<std::shared_mutex> lock(m_mutex);
        m_address = address;
    };

    void SetRequestPort(NodeUtils::PortNumber const& port)
    {
        std::unique_lock<std::shared_mutex> lock(m_mutex);
        m_requestPort = port;
    };

    void SetPublisherPort(NodeUtils::PortNumber const& port)
    {
        std::unique_lock<std::shared_mutex> lock(m_mutex);
        m_publisherPort = port;
    };

    void SetTechnology(NodeUtils::TechnologyType technology)
    {
        std::unique_lock<std::shared_mutex> lock(m_mutex);
        m_technology = technology;
    };

private:
    NodeUtils::NodeIdType m_id;    // Coordinator identification number of the node's coordinator
    
    NodeUtils::NetworkAddress m_address;
    NodeUtils::PortNumber m_requestPort;
    NodeUtils::PortNumber m_publisherPort;

    std::string m_requestEntry; // The combination of the address and request port 
    std::string m_publisherEntry; // The combination of the address and publisher port

    NodeUtils::TechnologyType m_technology;

    mutable std::shared_mutex m_mutex;
};

//------------------------------------------------------------------------------------------------

class State::CNetwork {
public:
    CNetwork()
        : m_peerNames()
        , m_uptime()
        , m_registered()
        , m_updated()
        , m_mutex()
    {
    };

    std::set<NodeUtils::NodeIdType> GetPeerNames() const 
    {
        std::shared_lock<std::shared_mutex> lock(m_mutex);
        return m_peerNames;
    };

    std::size_t GetKnownNodes() const
    {
        std::shared_lock<std::shared_mutex> lock(m_mutex);
        return m_peerNames.size();
    };

    NodeUtils::TimePeriod GetUptimeCount() const
    {
        std::shared_lock<std::shared_mutex> lock(m_mutex);
        return m_uptime;
    };

    NodeUtils::TimePoint GetRegisteredTimePoint() const
    {
        std::shared_lock<std::shared_mutex> lock(m_mutex);
        return m_registered;
    }

    NodeUtils::TimePoint GetUpdatedTimePoint() const
    {
        std::shared_lock<std::shared_mutex> lock(m_mutex);
        return m_updated;
    }

    void PushPeerName(NodeUtils::NodeIdType const& peerName)
    {
        std::unique_lock<std::shared_mutex> lock(m_mutex);
        if (auto const itr = m_peerNames.find(peerName); itr != m_peerNames.end()) {
            m_peerNames.emplace(peerName);
            m_updated = NodeUtils::GetSystemTimePoint();
        }
    }

    void RemovePeerName(NodeUtils::NodeIdType const& peerName)
    {
        std::unique_lock<std::shared_mutex> lock(m_mutex);
        if (auto const itr = m_peerNames.find(peerName); itr != m_peerNames.end()) {
            m_peerNames.erase(itr);
            m_updated = NodeUtils::GetSystemTimePoint();
        } 
    }

    void SetRegisteredTimePoint(NodeUtils::TimePoint const& timepoint)
    {
        std::unique_lock<std::shared_mutex> lock(m_mutex);
        m_registered = timepoint;
    };

    void Updated()
    {
        std::unique_lock<std::shared_mutex> lock(m_mutex);
        m_updated = NodeUtils::GetSystemTimePoint();
    };

private:
    std::set<NodeUtils::NodeIdType> m_peerNames;

    NodeUtils::TimePeriod m_uptime; // The amount of time the node has been live
    NodeUtils::TimePoint m_registered; // The timestamp the node was added to the network
    NodeUtils::TimePoint m_updated;    // The timestamp the node was last updated

    mutable std::shared_mutex m_mutex;
};

//------------------------------------------------------------------------------------------------

class State::CSecurity {
public:
    CSecurity()
        : m_standard()
        , m_mutex()
    {
    };

    explicit CSecurity(std::string_view protocol)
        : m_standard(protocol)
        , m_mutex()
    {
    };

    std::string GetProtocol() const
    {
        std::shared_lock<std::shared_mutex> lock(m_mutex);
        return m_standard;
    };

    void SetProtocol(std::string const& protocol)
    {
        std::unique_lock<std::shared_mutex> lock(m_mutex);
        m_standard = protocol;
    }

private:
    std::string m_standard;

    mutable std::shared_mutex m_mutex;
};

//------------------------------------------------------------------------------------------------

class State::CSelf {
public:
    CSelf()
        : m_id(0)
        , m_serial(std::string())
        , m_address(std::string())
        , m_port(std::string())
        , m_binding(std::string())
        , m_nextAvailablePort(0)
        , m_cluster(0)
        , m_operation(NodeUtils::DeviceOperation::None)
        , m_technologies()
        , m_mutex()
    {
    };

    CSelf(
        std::string_view interface,
        NodeUtils::AddressComponentPair const& bindingComponents,
        NodeUtils::DeviceOperation operation,
        std::set<NodeUtils::TechnologyType> const& technologies)
        : m_id(0) // TODO: Set Machine UUID for state
        , m_serial(std::string())
        , m_address(std::string())
        , m_port(bindingComponents.second)
        , m_binding(std::string())
        , m_publisherPort(std::string())
        , m_nextAvailablePort(0)
        , m_cluster(0)
        , m_operation(operation)
        , m_technologies(technologies)
        , m_mutex()
    {
        // Determine the actual IP address of the node based on the interface 
        // provided. It is currently assumed, the address provided in the binding
        // components is a wildcard character that needs expanding.
        m_address = NodeUtils::GetLocalAddress(interface);

        // Construct a full binding ip:port from the address 
        m_binding = m_address + ":" + m_port;

        std::int32_t const portNumber = std::stoi(bindingComponents.second);
        m_publisherPort = std::to_string(portNumber + 1);
        m_nextAvailablePort = portNumber + NodeUtils::PORT_GAP;
    };

    NodeUtils::NodeIdType GetId() const
    {
        std::shared_lock<std::shared_mutex> lock(m_mutex);
        return m_id;
    };

    NodeUtils::SerialNumber GetSerial() const
    {
        std::shared_lock<std::shared_mutex> lock(m_mutex);
        return m_serial;
    };

    std::string GetBinding() const
    {
        std::shared_lock<std::shared_mutex> lock(m_mutex);
        return m_binding;
    };

    NodeUtils::NetworkAddress GetAddress() const
    {
        std::shared_lock<std::shared_mutex> lock(m_mutex);
        return m_address;
    };

    NodeUtils::PortNumber GetPort() const
    {
        std::shared_lock<std::shared_mutex> lock(m_mutex);
        return m_port;
    };

    NodeUtils::PortNumber GetPublisherPort() const
    {
        std::shared_lock<std::shared_mutex> lock(m_mutex);
        return m_publisherPort;
    };

    std::uint32_t GetNextPort()
    {
        std::shared_lock<std::shared_mutex> lock(m_mutex);
        return ++m_nextAvailablePort; // TODO: Need smarter method of choosing the next port
    };

    NodeUtils::ClusterIdType GetCluster() const
    {
        std::shared_lock<std::shared_mutex> lock(m_mutex);
        return m_cluster;
    };

    NodeUtils::DeviceOperation GetOperation() const
    {
        std::shared_lock<std::shared_mutex> lock(m_mutex);
        return m_operation;
    };

    std::set<NodeUtils::TechnologyType> GetTechnologies() const
    {
        std::shared_lock<std::shared_mutex> lock(m_mutex);
        return m_technologies;
    };

    void SetId(NodeUtils::NodeIdType const& id)
    {
        std::unique_lock<std::shared_mutex> lock(m_mutex);
        m_id = id;
    }

    void SetSerial(NodeUtils::SerialNumber const& serial)
    {
        std::unique_lock<std::shared_mutex> lock(m_mutex);
        m_serial = serial;
    }

    void SetCluster(NodeUtils::ClusterIdType const& cluster)
    {
        std::unique_lock<std::shared_mutex> lock(m_mutex);
        m_cluster = cluster;
    }

    void SetOperation(NodeUtils::DeviceOperation operation)
    {
        std::unique_lock<std::shared_mutex> lock(m_mutex);
        m_operation = operation;
    }

    void SetTechnologies(std::set<NodeUtils::TechnologyType> const& technologies)
    {
        std::unique_lock<std::shared_mutex> lock(m_mutex);
        m_technologies = technologies;
    }

private:
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

    mutable std::shared_mutex m_mutex;
};

//------------------------------------------------------------------------------------------------

class State::CSensor {
public:
    CSensor()
        : m_pin()
        , m_mutex()
    {
    };

    std::uint8_t GetPin() const
    {
        std::shared_lock<std::shared_mutex> lock(m_mutex);
        return m_pin;
    };

    void SetPin(std::uint8_t pin)
    {
        std::unique_lock<std::shared_mutex> lock(m_mutex);
        m_pin = pin;
    }

private:
    std::uint8_t m_pin;   // The GPIO pin the node will read from

    mutable std::shared_mutex m_mutex;
};

//------------------------------------------------------------------------------------------------

class CState {
public:
    explicit CState(Configuration::TSettings const& settings)
        : m_authority(std::make_shared<State::CAuthority>(settings.security.central_authority))
        , m_coordinator(std::make_shared<State::CCoordinator>(
            settings.connections[0].technology,
            settings.connections[0].GetEntryComponents()))
        , m_network(std::make_shared<State::CNetwork>())
        , m_security(std::make_shared<State::CSecurity>(settings.security.standard))
        , m_self(std::make_shared<State::CSelf>(
            settings.connections[0].interface,
            settings.connections[0].GetBindingComponents(),
            settings.details.operation,
            std::set<NodeUtils::TechnologyType>{settings.connections[0].technology}
        ))
        , m_sensor(std::make_shared<State::CSensor>())
    {
    };

    std::weak_ptr<State::CAuthority> GetAuthorityState() const
    {
        return m_authority;
    };

    std::weak_ptr<State::CCoordinator> GetCoordinatorState() const
    {
        return m_coordinator;
    };

    std::weak_ptr<State::CNetwork> GetNetworkState() const
    {
        return m_network;
    };

    std::weak_ptr<State::CSecurity> GetSecurityState() const
    {
        return m_security;
    };

    std::weak_ptr<State::CSelf> GetSelfState() const
    {
        return m_self;
    };

    std::weak_ptr<State::CSensor> GetSensorState() const
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
