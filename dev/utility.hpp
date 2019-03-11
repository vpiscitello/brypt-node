#ifndef UTILITY_HPP
#define UTILITY_HPP

#include <iostream>
#include <string.h>
#include <string>
#include <chrono>
#include <thread>

enum DeviceOperation { ROOT, BRANCH, LEAF };

enum TechnologyType { DIRECT_TYPE, BLE_TYPE, LORA_TYPE, WEBSOCKET_TYPE, TCP_TYPE, NONE };

enum CommandType { INFORMATION_TYPE, QUERY_TYPE, ELECTION_TYPE, TRANSFORM_TYPE, CONNECT_TYPE, NULL_CMD };

enum DeviceSocketCapability { MASTER, SLAVE };

// Central Authority Connection Constants
const std::string CA_DOMAIN = "brypt.com";
const std::string CA_SUBDOMAIN = "bridge";
const std::string CA_PORT = "8080";
const std::string CA_PROTOCOL = "https://";
const unsigned int PORT_GAP = 16;

struct Options {
    bool run_tests;
    TechnologyType technology;
    DeviceOperation operation;
    std::string id;
    std::string addr;
    std::string port;

    std::string peer_name;
    std::string peer_addr;
    std::string peer_port;

    bool is_control;
};

inline char * cast_string( std::string s ) {
    char * cs = new char[ s.size() ];
    memset( cs, '\0', s.size() );
    strcpy( cs, s.c_str() );
    return cs;
}

#endif
