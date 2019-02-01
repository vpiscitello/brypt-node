#ifndef UTILITY_HPP
#define UTILITY_HPP

#include <string>

// Communication Type Constants
// const int DIRECT = 0;
// const int BLE = 1;
// const int LORA = 2;
// const int WEBSOCKET = 3;

enum DeviceOperation { SERVER, CLIENT };

enum TechnologyType { DIRECT_TYPE, BLE_TYPE, LORA_TYPE, WEBSOCKET_TYPE };

enum CommandType { INFORMATION_TYPE, QUERY_TYPE, ELECTION_TYPE, TRANSFORM_TYPE, CONNECT_TYPE };

// Central Authority Connection Constants
const std::string CA_DOMAIN = "brypt.com";
const std::string CA_SUBDOMAIN = "bridge";
const std::string CA_PORT = "8080";
const std::string CA_PROTOCOL = "https://";

struct Options {
    bool run_tests;
    TechnologyType technology;
    DeviceOperation operation;
    std::string IP;
    std::string port;
    std::string peer_IP;
    std::string peer_port;
};

#endif
