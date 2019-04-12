#include "utility.hpp"
#include "node.hpp"
#include "mqueue.hpp"
#include <sys/stat.h>
#include <fstream>
#include <fcntl.h>

#include <limits.h>

#include <string>

struct Options options;

void connection_factory_test() {
    std::cout << "\n== Testing Connection Factory" << '\n';
    // Setup a vector of open connections
    std::vector<Connection *> connections;
    connections.push_back( ConnectionFactory(DIRECT_TYPE) );
    connections.push_back( ConnectionFactory(BLE_TYPE) );
    connections.push_back( ConnectionFactory(LORA_TYPE) );
    connections.push_back( ConnectionFactory(DIRECT_TYPE) );

    // Check the connection type and run a shared function
    for (int idx = 0; idx < (int)connections.size(); idx++) {
        connections.at(idx)->whatami();
        connections.at(idx)->unspecial();
        std::cout << '\n';
    }
}

void command_parse_test() {
    std::cout << "\n== Testing Command Parsing" << '\n';
    // Setup the vector of commands to be used
    Node node;
    State state;
    std::vector<Command *> commands;
    commands.push_back( CommandFactory(INFORMATION_TYPE, &node, &state) );
    commands.push_back( CommandFactory(QUERY_TYPE, &node, &state) );
    commands.push_back( CommandFactory(ELECTION_TYPE, &node, &state) );
    commands.push_back( CommandFactory(TRANSFORM_TYPE, &node, &state) );
    commands.push_back( CommandFactory(CONNECT_TYPE, &node, &state) );

    // Setup a new message to match to a command
    CommandType command = ELECTION_TYPE;
    int phase = 0;
    std::string node_id = "00-00-00-00-00";
    std::string data = "Hello World!";
    unsigned int nonce = 998;
    Message message( node_id, "", command, phase, data, nonce );

    // Use the message command to handle the message logic
    commands.at( message.get_command() )->handle_message( &message );

    if ( message.get_response() == "" ) {
        std::cout << "Command has no Response." << '\n';
    } else {
        std::cout << "Command has no Response." << '\n';
    }


}

void message_queue_test() {
	std::cout << "\n== Testing Message Queue" << '\n';

	MessageQueue message_queue;

	int phase = 0;
	std::string plaintext("H\0el\0lo, Wo\0rld\0",16);
	std::string node_id = "00-00-00-00-01";
	CommandType command = ELECTION_TYPE;
	unsigned int nonce = 998;

	Message wrapper( node_id, "", command, phase, plaintext, nonce );    // Create the message using known data
	std::string packet = wrapper.get_pack();
	std::cout << packet <<"\n";

	message_queue.push_pipe("1");
	std::fstream myfile("1", std::ios::out | std::ios::binary);

	for(int i = 0; i < (int)packet.size(); i++){
		myfile.put(packet.at(i));
	}

	myfile.close();

	message_queue.check_pipes();
    Message tmpmsg = message_queue.pop_next_message();
	if( tmpmsg.get_phase() != UINT_MAX ){
		std::cout << "Pop msg \n" << tmpmsg.get_pack() << '\n';
	}

	// message_queue.add_message(wrapper);
    //
	// message_queue.push_pipes();
	// message_queue.check_pipes();
    //
	// message_queue.push_pipe("3");
	// tmpmsg.set_node_id("5");
	// message_queue.add_message(tmpmsg);
	// message_queue.remove_pipe("5");
}

void message_test() {
    std::cout << "\n== Testing Messages" << '\n';

    // Setup a new message
    CommandType command = ELECTION_TYPE;    // Set the message command type
    int phase = 0;                          // Set the message command phase
    std::string node_id = "00-00-00-00-00"; // Set the message sender ID
    std::string data = "Hello World!";      // Set the message data
    unsigned int nonce = 998;               // Set the message key nonce
    Message message( node_id, "", command, phase, data, nonce );    // Create the message using known data

    std::string recv_raw = message.get_pack();      // Get the message as a packed string
    std::cout << "Message Raw: " << recv_raw << '\n';

    Message recv_message( recv_raw );       // Initialize a new message using a recieved raw string
    std::cout << "Message Sender: "  << recv_message.get_source_id() << '\n';
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
    connection_factory_test();
    command_parse_test();
    message_queue_test();
    message_test();
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
    it = find (args.begin(), args.end(), "--root");
    if (it != args.end()) {
        options.operation = ROOT;
    } else {
        // Parse node function. Client option.
        it = find (args.begin(), args.end(), "--branch");
        if (it != args.end()) {
            options.operation = BRANCH;
        } else {
            it = find (args.begin(), args.end(), "--leaf");
            if (it != args.end()) {
                options.operation = LEAF;
            } else {
                std::cout << "== You must specify node function." << '\n';
                exit(1);
            }
        }
    }

    // Parse node's port to open
    it = find (args.begin(), args.end(), "-id");
    if (it != args.end()) {
        it++;
        if (*it == "" || it->find("-") != std::string::npos) {
            std::cout << "== You must specify an ID." << '\n';
            exit(1);
        } else {
            options.id = *it;
        }
    } else {
        std::cout << "== You must specify an ID." << '\n';
        exit(1);
    }

    // Parse device's connection technology
    it = find (args.begin(), args.end(), "-type");
    if (it != args.end()) {
        it++;
        if (*it == "" || it->find("-") != std::string::npos) {
            std::cout << "== You must specify a device type." << '\n';
            exit(1);
        } else {
            std::string device_type = *it;
            if (device_type == "DIRECT") {
                options.technology = DIRECT_TYPE;
            } else if (device_type == "BLE") {
                options.technology = BLE_TYPE;
            } else if (device_type == "LORA") {
                options.technology = LORA_TYPE;
            } else {
                std::cout << "== Invalid device type." << '\n';
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
    if (options.operation == BRANCH || options.operation == LEAF) {
        // Parse server peer's address
        it = find (args.begin(), args.end(), "-peer");
        if (it != args.end()) {
            it++;
            if (*it == "" || it->find("-") != std::string::npos) {
                std::cout << "== You must specify a peer address." << '\n';
                exit(1);
            } else {
                options.peer_addr = *it;
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

void create_tcp_socket() {
    Options tcp_setup;
    tcp_setup.technology = TCP_TYPE;
    tcp_setup.port = "3001";
    tcp_setup.operation = ROOT;
    Connection * conn = ConnectionFactory(tcp_setup.technology, &tcp_setup);
    conn->recv(1);
    conn->send("THIS IS A MESSAGE");
}

void create_tcp_connection() {
    Options tcp_setup;
    tcp_setup.technology = TCP_TYPE;
    tcp_setup.peer_addr = "127.0.0.1";
    tcp_setup.peer_port = "3001";
    tcp_setup.operation = LEAF;
    Connection * conn = ConnectionFactory(tcp_setup.technology, &tcp_setup);
    conn->send("THIS IS A MESSAGE");
    conn->recv(1);
}

void create_streambridge_socket() {
    Options streambridge_setup;
    streambridge_setup.technology = STREAMBRIDGE_TYPE;
    streambridge_setup.port = "3001";
    streambridge_setup.operation = ROOT;
    Connection * conn = ConnectionFactory(streambridge_setup.technology, &streambridge_setup);
    conn->recv(1);
    conn->send("THIS IS A MESSAGE");
}

int main(int argc, char **argv) {

    parse_args(argc, argv);

    if (options.run_tests == true) {
        run_tests();
        exit(0);
    }

    std::cout << "\n== Welcome to the Brypt Network\n";

    // create_tcp_socket();
    // create_streambridge_socket();
    // create_tcp_connection();
    // return 0;

    class Node alpha;

    std::string local_ip = alpha.get_local_address();
    std::cout << "Local Connection IPV4: " << local_ip << '\n';
    std::cout << "Main process PID: " << getpid() << "\n";

    alpha.setup( options );

    alpha.startup();

    return 0;
}
