#ifndef STATE_HPP
#define STATE_HPP

#include <ctime>
#include <string>
#include "utility.hpp"

struct Self {
    DeviceOperation operation = NO_OPER;  // A boolean value of the node's root status
    std::string id = "";    // Network identification number of the node
    std::string serial = "";    // Hardset identification number of the device
    std::string addr = "";  // IP address of the node
    std::string port = "";  // IP address of the node
    std::string control_port = "";  // Networking port of the node
    unsigned int next_full_port = 0;
    std::vector<TechnologyType> available_technologies; // Communication technologies of the node
    std::string cluster = "";   // Cluster identification number of the node's cluster
};

struct Coordinator {
    TechnologyType technology = NO_TECH;
    std::string id = "";    // Coordinator identification number of the node's coordinator
    std::string addr = "";
    std::string request_port = "";
    std::string publisher_port = "";
};

struct Authority {
    std::string addr = "";  // Networking address of the central authority for the Brypt ecosystem
    std::string token = ""; // Access token for the Brypt network
};

struct Network {
    unsigned int known_nodes = 0;   // The number of nodes the node has been in contact with
    std::time_t uptime; // The amount of time the node has been live
    std::time_t registered; // The timestamp the node was added to the network
    std::time_t updated;    // The timestamp the node was last updated
};

struct Sensor {
    unsigned int pin = 0;   // The GPIO pin the node will read from
};

struct Security {
    std::string protocol = "";
};


struct State {
    Self self;
    Coordinator coordinator;
    Authority authority;
    Network network;
    Sensor sensor;
    Security security;
};

#endif
