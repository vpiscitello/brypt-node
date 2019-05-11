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
enum PrintType { AWAIT_P, COMMAND_P, CONNECTION_P, CONTROL_P, MESSAGE_P, MQUEUE_P, NODE_P, NOTIFIER_P, WATCHER_P, ERROR_P };

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
    std::chrono::milliseconds milliseconds;
    SystemClock current_time;
    current_time = std::chrono::system_clock::now();
    milliseconds = std::chrono::duration_cast<std::chrono::milliseconds>( current_time.time_since_epoch() );
    epoch_ss.clear();
    epoch_ss << milliseconds.count();
    return epoch_ss.str();
}

inline std::string get_print_escape(PrintType component) {
    std::string escape = "";
    switch (component) {
        case AWAIT_P: {
            escape = "\e[1;30;48;5;93m[    Await    ]\e[0m ";
            break;
        }
        case COMMAND_P: {
            escape = "\e[1;30;48;5;220m[   Command   ]\e[0m ";
            break;
        }
        case CONNECTION_P: {
            escape = "\e[1;30;48;5;6m[  Connection ]\e[0m ";
            break;
        }
        case CONTROL_P: {
            escape = "\e[1;97;48;5;4m[   Control   ]\e[0m ";
            break;
        }
        case MESSAGE_P: {
            escape = "\e[1;30;48;5;135m[   Message   ]\e[0m ";
            break;
        }
        case MQUEUE_P: {
            escape = "\e[1;30;48;5;129m[ MessageQueue ]\e[0m ";
            break;
        }
        case NODE_P: {
            escape = "\e[1;30;48;5;42m[     Node    ]\e[0m ";
            break;
        }
        case NOTIFIER_P: {
            escape = "\e[1;30;48;5;12m[   Notifier  ]\e[0m ";
            break;
        }
        case WATCHER_P: {
            escape = "\e[1;30;48;5;203m[ PeerWatcher ]\e[0m ";
            break;
        }
    }
    return escape;
}

inline void printo(std::string message, PrintType component) {
	std::string escape = get_print_escape(component);
	std::cout << "== " << escape << message << std::endl;
}

#endif
