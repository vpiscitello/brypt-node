#ifndef UTILITY_HPP
#define UTILITY_HPP

#include <iostream>
#include <string.h>
#include <string>
#include <sstream>
#include <chrono>

#include <thread>
#include <mutex>
#include <condition_variable>

typedef std::chrono::time_point<std::chrono::system_clock> SystemClock;

enum DeviceOperation { ROOT, BRANCH, LEAF, NO_OPER };
enum DeviceSocketCapability { MASTER, SLAVE };
enum TechnologyType { DIRECT_TYPE, BLE_TYPE, LORA_TYPE, TCP_TYPE, STREAMBRIDGE_TYPE, NO_TECH };
enum CommandType { INFORMATION_TYPE, QUERY_TYPE, ELECTION_TYPE, TRANSFORM_TYPE, CONNECT_TYPE, NO_CMD };
enum NotificationType { NETWORK_NOTICE, CLUSTER_NOTICE, NODE_NOTICE, NO_NOTICE };

// Super Secure NET_KEY
const std::string NET_KEY = "01234567890123456789012345678901";
const unsigned int NET_NONCE = 998;

// Central Authority Connection Constants
const std::string CA_DOMAIN = "brypt.com";
const std::string CA_SUBDOMAIN = "bridge";
const std::string CA_PORT = "8080";
const std::string CA_PROTOCOL = "https://";
const unsigned int PORT_GAP = 16;

const std::string ID_SEPERATOR = ";";

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

inline std::string get_designation(DeviceOperation operation) {
    std::string designation = "";
    switch (operation) {
        case ROOT: {
            designation = "root";
            break;
        }
        case BRANCH: {
            designation = "coordinator";
            break;
        }
        case LEAF: {
            designation = "node";
            break;
        }
        case NO_OPER: {
            designation = "NA";
            break;
        }
    }
    return designation;
}

inline SystemClock get_system_clock() {
    return std::chrono::system_clock::now();
}

inline std::string get_system_timestamp() {
    std::stringstream epoch_ss;
    std::chrono::seconds seconds;
    SystemClock current_time;
    current_time = std::chrono::system_clock::now();
    seconds = std::chrono::duration_cast<std::chrono::seconds>( current_time.time_since_epoch() );
    epoch_ss.clear();
    epoch_ss << seconds.count();
    return epoch_ss.str();
}

#endif
