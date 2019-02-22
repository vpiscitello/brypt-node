#include "utility.hpp"
#include "node.hpp"
#include "mqueue.hpp"
#include <sys/stat.h>
#include <fcntl.h> 

struct Options options;

void connection_factory_test() {
    std::cout << "\n== Testing Connection Factory" << '\n';
    // Setup a vector of open connections
    std::vector<Connection *> connections;
    connections.push_back( ConnectionFactory(DIRECT_TYPE) );
    connections.push_back( ConnectionFactory(BLE_TYPE) );
    connections.push_back( ConnectionFactory(LORA_TYPE) );
    connections.push_back( ConnectionFactory(DIRECT_TYPE) );
    connections.push_back( ConnectionFactory(WEBSOCKET_TYPE) );

    // Check the connection type and run a shared function
    for (int idx = 0; idx < connections.size(); idx++) {
        connections.at(idx)->whatami();
        connections.at(idx)->unspecial();
        std::cout << '\n';
    }
}

void message_command_parse_test() {
    std::cout << "\n== Testing Command Parsing" << '\n';
    // Setup the vector of commands to be used
    std::vector<Command *> commands;
    commands.push_back( CommandFactory(INFORMATION_TYPE) );
    commands.push_back( CommandFactory(QUERY_TYPE) );
    commands.push_back( CommandFactory(ELECTION_TYPE) );
    commands.push_back( CommandFactory(TRANSFORM_TYPE) );
    commands.push_back( CommandFactory(CONNECT_TYPE) );

    // Setup a new message to match to a command
    CommandType command = ELECTION_TYPE;
    int phase = 0;
    std::string node_id = "00-00-00-00-00";
    std::string data = "Hello World!";
    unsigned int nonce = 998;
    Message message( node_id, command, phase, data, nonce );

    // Use the message command to handle the message logic
    commands.at( message.get_command() )->handle_message( &message );

    if ( message.get_response() == "" ) {
        std::cout << "Command has no Response." << '\n';
    } else {
        std::cout << "Command has no Response." << '\n';
    }


}
void message_queue_test() {
    int fd;
    std::cout << "\n== Testing Message Queue" << '\n';
    MessageQueue message_queue;
    message_queue.addPipe("1");
    fd = open("1",O_WRONLY|O_APPEND);
    printf("%d\n", write(fd, "\0Hell\0o, World\0",(ssize_t)12));
    close(fd);
    printf("%d",fd);
    message_queue.checkPipes();
}
void message_message_test() {
    std::cout << "\n== Testing Messages" << '\n';

    // Setup a new message
    CommandType command = ELECTION_TYPE;    // Set the message command type
    int phase = 0;                          // Set the message command phase
    std::string node_id = "00-00-00-00-00"; // Set the message sender ID
    std::string data = "Hello World!";      // Set the message data
    unsigned int nonce = 998;               // Set the message key nonce
    Message message( node_id, command, phase, data, nonce );    // Create the message using known data

    std::string recv_raw = message.get_pack();      // Get the message as a packed string
    std::cout << "Message Raw: " << recv_raw << '\n';

    Message recv_message( recv_raw );       // Initialize a new message using a recieved raw string
    std::cout << "Message Sender: "  << recv_message.get_node_id() << '\n';
    std::cout << "Message Content: "  << recv_message.get_data() << '\n';

    // TODO: verify message
    bool verified = recv_message.verify();      // Verify the message by checking the HMAC
    if (verified) {
        std::cout << "Message Verification: Success!" << '\n';
    } else {
        std::cout << "Message Verification: Tampered!" << '\n';
    }

    std::string resp_node_id = "11-11-11-11-11";             // Set the message response sender ID
    std::string resp_data = "Re: Hello World! - Hi.";        // Set the message response data
    recv_message.set_response( resp_node_id, resp_data );   // Set the message's response using the known data
    std::cout << "Message Response: "  << recv_message.get_response() << "\n\n";

    std::string tampered_raw = recv_raw;    // Get the raw message string to tamper
    tampered_raw.at( 49 ) = '?';            // Replace a character in the data
    std::cout << "Tampered Message: " << tampered_raw << '\n';
    Message tamp_message( tampered_raw );       // Create a tampered message
    std::cout << "Tampered Content: "  << tamp_message.get_data() << '\n';

    bool tamp_verified = tamp_message.verify();     // Verify the tampered message by checking the HMAC
    if (tamp_verified) {
        std::cout << "Message Verification: Success!\n" << '\n';
    } else {
        std::cout << "Message Verification: Tampered!\n" << '\n';
    }
}

void run_tests() {
    //connection_factory_test();
    //message_command_parse_test();
    message_queue_test();
    //message_message_test();
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
        options.operation = SERVER;
    } else {
        // Parse node function. Client option.
        it = find (args.begin(), args.end(), "--client");
        if (it != args.end()) {
            options.operation = CLIENT;
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
                options.technology = DIRECT_TYPE;
            } else if (device_type == "BLE") {
                options.technology = BLE_TYPE;
            } else if (device_type == "LORA") {
                options.technology = LORA_TYPE;
            } else if (device_type == "WEBSOCKET") {
                options.technology = WEBSOCKET_TYPE;
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
    if (options.operation == CLIENT) {
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

    alpha.setup( options );

    return 0;
}
