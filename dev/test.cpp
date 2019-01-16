#include "utility.hpp"
#include "node.hpp"

enum DeviceFunctions { SERVER, CLIENT };

struct ProgramOptions {
    bool run_tests;
    int device_type;
    int device_function;
    std::string port;
    std::string peer_IP;
    std::string peer_port;
};

struct ProgramOptions options;

void connection_factory_test() {
    std::cout << "== Testing Connection Factory" << '\n';
    std::vector<Connection *> connections;
    connections.push_back( ConnectionFactory(DIRECT_TYPE) );
    connections.push_back( ConnectionFactory(BLE_TYPE) );
    connections.push_back( ConnectionFactory(LORA_TYPE) );
    connections.push_back( ConnectionFactory(DIRECT_TYPE) );
    connections.push_back( ConnectionFactory(WEBSOCKET_TYPE) );

    for (int idx = 0; idx < connections.size(); idx++) {
        connections.at(idx)->whatami();
        connections.at(idx)->unspecial();
        std::cout << '\n';
    }
}

void run_tests() {
    connection_factory_test();
}

void parse_args(int argc, char **argv) {
    std::vector<std::string> args;
    std::vector<std::string>::iterator it;

    if (argc <= 1) { return; }

    for(int idx = 0; idx < argc; idx++) {
        args.push_back(std::string(argv[idx]));
    }

    // Parse test options
    it = find (args.begin(), args.end(), "--test");
    if (it != args.end()) {
        options.run_tests = true;
        return;
    } else {
        options.run_tests = false;
    }

    // Parse node function. Server option.
    it = find (args.begin(), args.end(), "--server");
    if (it != args.end()) {
        options.device_function = SERVER;
    } else {
        // Parse node function. Client option.
        it = find (args.begin(), args.end(), "--client");
        if (it != args.end()) {
            options.device_function = CLIENT;
        } else {
            std::cout << "== You must specify node function." << '\n';
            exit(1);
        }
    }

    // Parse device's connection technology
    it = find (args.begin(), args.end(), "-type");
    if (it != args.end()) {
        it++;
        if (*it == "" || it->find("-") != std::string::npos) {
            std::cout << "== You must specify a devic type." << '\n';
            exit(1);
        } else {
            std::string device_type = *it;
            if (device_type == "DIRECT") {
                options.device_type = DIRECT_TYPE;
            } else if (device_type == "BLE") {
                options.device_type = BLE_TYPE;
            } else if (device_type == "LORA") {
                options.device_type = LORA_TYPE;
            } else if (device_type == "WEBSOCKET") {
                options.device_type = WEBSOCKET_TYPE;
            } else {
                std::cout << "== Invalid devic type." << '\n';
                exit(1);
            }
        }
    } else {
        std::cout << "== You must specify a devic type." << '\n';
        exit(1);
    }

    // Parse node's port to open
    it = find (args.begin(), args.end(), "-port");
    if (it != args.end()) {
        it++;
        if (*it == "" || it->find("-") != std::string::npos) {
            std::cout << "== You must specify a port to open." << '\n';
            exit(1);
        } else {
            options.port = *it;
        }
    } else {
        std::cout << "== You must specify a port to open." << '\n';
        exit(1);
    }

    // Parse Client specific options
    if (options.device_function == CLIENT) {
        // Parse server peer's address
        it = find (args.begin(), args.end(), "-peer");
        if (it != args.end()) {
            it++;
            if (*it == "" || it->find("-") != std::string::npos) {
                std::cout << "== You must specify a peer address." << '\n';
                exit(1);
            } else {
                options.peer_IP = *it;
            }
        } else {
            std::cout << "== You must specify a peer address." << '\n';
            exit(1);
        }

        // Parse server peer's port
        it = find (args.begin(), args.end(), "-pp");
        if (it != args.end()) {
            it++;
            if (*it == "" || it->find("-") != std::string::npos) {
                std::cout << "== You must specify the peer's port." << '\n';
                exit(1);
            } else {
                options.peer_port = *it;
            }
        }
        else {
            std::cout << "== You must specify the peer's port." << '\n';
            exit(1);
        }

    }


}

int main(int argc, char **argv) {

    parse_args(argc, argv);

    if (options.run_tests == true) {
        run_tests();
        exit(0);
    }

    std::cout << "== Starting Brypt Node" << std::endl;

    class Node alpha;
    std::string local_ip = alpha.get_local_address();
    std::cout << "Local Connection IPV4: " << local_ip << '\n';

    if (options.device_function == SERVER) {
        alpha.listen();
    } else {

    }

    return 0;
}
