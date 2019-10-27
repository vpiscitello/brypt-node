//------------------------------------------------------------------------------------------------
// File: Test.cpp
// Description:
//------------------------------------------------------------------------------------------------
#include "Node.hpp"
#include "State.hpp"
#include "Components/Command/Handler.hpp"
#include "Components/Connection/Connection.hpp"
#include "Components/MessageQueue/MessageQueue.hpp"
#include "Utilities/NodeUtils.hpp"
//------------------------------------------------------------------------------------------------
#include <fcntl.h>
#include <unistd.h>
#include <cstdint>
#include <climits>
#include <cstdint>
#include <fstream>
#include <chrono>
#include <memory>
#include <optional>
#include <string>
#include <thread>
#include <sys/stat.h>
#include <sys/types.h>
//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------

NodeUtils::TOptions options;

//------------------------------------------------------------------------------------------------

// TODO: Move unit tests into specific unit testing folder
// TODO: Add more unit tests

void ConnectionFactoryTest() {
    std::cout << "\n== Testing Connection Factory" << '\n';
    // Setup a vector of open connections
    std::vector<std::shared_ptr<CConnection>> connections;
    auto const invalid = Connection::Factory(NodeUtils::TechnologyType::NONE);
    if (!invalid) {
        std::cout << "Building a connection with type none failed successfully" << std::endl;
    }

    connections.push_back(Connection::Factory(NodeUtils::TechnologyType::DIRECT));
    connections.push_back(Connection::Factory(NodeUtils::TechnologyType::LORA));
    connections.push_back(Connection::Factory(NodeUtils::TechnologyType::STREAMBRIDGE));
    connections.push_back(Connection::Factory(NodeUtils::TechnologyType::TCP));
    connections.push_back(Connection::Factory(NodeUtils::TechnologyType::DIRECT));
    
    // Check the connection type and run a shared function
    for (std::size_t idx = 0; idx < connections.size(); ++idx) {
        connections.at(idx)->whatami();
    }
}

//------------------------------------------------------------------------------------------------

void CommandParseTest() {
    std::cout << "\n== Testing Command Parsing" << '\n';
    // Setup the vector of commands to be used
    CNode node(options);
    std::shared_ptr<CState> state = std::make_shared<CState>(options);
    std::vector<std::unique_ptr<Command::CHandler>> commands;
    commands.push_back(Command::Factory(NodeUtils::CommandType::INFORMATION, node, state));
    commands.push_back(Command::Factory(NodeUtils::CommandType::QUERY, node, state));
    commands.push_back(Command::Factory(NodeUtils::CommandType::ELECTION, node, state));
    commands.push_back(Command::Factory(NodeUtils::CommandType::TRANSFORM, node, state));
    commands.push_back(Command::Factory(NodeUtils::CommandType::CONNECT, node, state));

    // Setup a new message to match to a command
    CMessage const message(
        "0xFFFFFFFF", "0x00000000",
        NodeUtils::CommandType::ELECTION, 0,
        "This is an election request message.", 9999);

    // Use the message command to handle the message logic
    commands.at(static_cast<std::int32_t>(message.GetCommand()))->HandleMessage(message);

    if (!message.GetResponse()) {
        std::cout << "Command has no Response." << '\n';
    } else {
        std::cout << "Command has no Response." << '\n';
    }
}

//------------------------------------------------------------------------------------------------

void MessageQueueTest() {
	std::cout << "\n== Testing Message Queue" << '\n';

	CMessageQueue msgQueue;

	std::string const nodeId = "0xFFFFFFFF";
    CMessage const message(
        nodeId, "0x00000000",
        NodeUtils::CommandType::ELECTION, 0,
        std::string("H\0el\0lo, Wo\0rld\0",16), 9999);

	msgQueue.AddManagedPipe(nodeId);

    std::string const packet = message.GetPack();
	std::cout << packet << std::endl;
	std::fstream file("./tmp/" + nodeId + ".pipe", std::ios::out | std::ios::binary);
	for(std::size_t idx = 0; idx < packet.size(); ++idx){
		file.put(packet[idx]);
	}
	file.close();

	msgQueue.CheckPipes();
    std::optional<CMessage> optIncomingMessage = msgQueue.PopIncomingMessage();
	if(optIncomingMessage && optIncomingMessage->GetPhase() != std::numeric_limits<std::uint32_t>::max()){
		std::cout << "Pop msg \n" << optIncomingMessage->GetPack() << '\n';
	}
}

//------------------------------------------------------------------------------------------------

void MessageTest() {
    std::cout << "\n== Testing Messages" << '\n';

    // Setup a new message
    CMessage const message(
        "0xFFFFFFFF", "0x00000000",
        NodeUtils::CommandType::ELECTION, 0,
        "Hello World!", 9999);

    std::string const receiveRaw = message.GetPack();      // Get the message as a packed string
    std::cout << "Message Raw: " << receiveRaw << '\n';

    CMessage receiveMessage(receiveRaw);       // Initialize a new message using a received raw string
    std::cout << "Message Sender: "  << receiveMessage.GetSourceId() << '\n';
    std::cout << "Message Content: "  << receiveMessage.GetData() << '\n';

    // TODO: verify message
    bool const isVerified = receiveMessage.Verify();      // Verify the message by checking the HMAC
    if (isVerified) {
        std::cout << "Message Verification: Success!" << '\n';
    } else {
        std::cout << "Message Verification: Tampered!" << '\n';
    }

    receiveMessage.SetResponse("0x00000000", "Re: Hello World! - Hi.");   // Set the message's response using the known data
    std::cout << "Message Response: " << receiveMessage.GetResponse().value() << "\n\n";

    std::string tamperedRaw = receiveRaw;    // Get the raw message string to tamper
    tamperedRaw.at(49) = '?';            // Replace a character in the data
    std::cout << "Tampered Message: " << tamperedRaw << '\n';
    CMessage const tamperedMessage(tamperedRaw);       // Create a tampered message
    std::cout << "Tampered Content: "  << tamperedMessage.GetData() << '\n';

    bool isTamperedVerified = tamperedMessage.Verify();     // Verify the tampered message by checking the HMAC
    if (isTamperedVerified) {
        std::cout << "Message Verification: Success!\n" << '\n';
    } else {
        std::cout << "Message Verification: Tampered!\n" << '\n';
    }
}

//------------------------------------------------------------------------------------------------

void RunTests() {
    options.m_id = "0xFFFFFFFF";
    options.m_interface = "en0";
    options.m_port = "3005";
    options.m_operation = NodeUtils::DeviceOperation::ROOT;
    options.m_technology = NodeUtils::TechnologyType::DIRECT;
    options.m_peerName = "0x00000000";
    options.m_peerAddress = "127.0.0.1";
    options.m_peerPort = "9999";

    ConnectionFactoryTest();
    CommandParseTest();
    MessageQueueTest();
    MessageTest();
}

//------------------------------------------------------------------------------------------------

void ParseArguments(std::int32_t argc, char** argv) {
    std::vector<std::string> args;
    std::vector<std::string>::iterator itr;

    if (argc <= 1) {
        NodeUtils::printo("Arguments must be provided!", NodeUtils::PrintType::ERROR);
        exit(1);
    }

    for(std::int32_t idx = 0; idx < argc; ++idx) {
        args.push_back(std::string(argv[idx]));
    }

    // Parse test options
    itr = find (args.begin(), args.end(), "--test");
    if (itr != args.end()) {
        options.m_runTests = true;
        return;
    } else {
        options.m_runTests = false;
    }

    // Parse node function. Server option.
    itr = find (args.begin(), args.end(), "--root");
    if (itr != args.end()) {
        options.m_operation = NodeUtils::DeviceOperation::ROOT;
    } else {
        // Parse node function. Client option.
        itr = find (args.begin(), args.end(), "--branch");
        if (itr != args.end()) {
            options.m_operation = NodeUtils::DeviceOperation::BRANCH;
        } else {
            itr = find (args.begin(), args.end(), "--leaf");
            if (itr != args.end()) {
                options.m_operation = NodeUtils::DeviceOperation::LEAF;
            } else {
                std::cout << "== You must specify node function." << '\n';
                exit(1);
            }
        }
    }

    // Parse node's port to open
    itr = find (args.begin(), args.end(), "-id");
    if (itr != args.end()) {
        ++itr;
        if (*itr == "" || itr->find("-") != std::string::npos) {
            std::cout << "== You must specify an ID." << '\n';
            exit(1);
        } else {
            options.m_id = *itr;
        }
    } else {
        std::cout << "== You must specify an ID." << '\n';
        exit(1);
    }

    // Parse device's connection technology
    itr = find (args.begin(), args.end(), "-type");
    if (itr != args.end()) {
        ++itr;
        if (*itr == "" || itr->find("-") != std::string::npos) {
            std::cout << "== You must specify a device type." << '\n';
            exit(1);
        } else {
            std::string device_type = *itr;
            if (device_type == "DIRECT") {
                options.m_technology = NodeUtils::TechnologyType::DIRECT;
            } else if (device_type == "LORA") {
                options.m_technology = NodeUtils::TechnologyType::LORA;
            } else if (device_type == "STREAMBRIDGE") {
                options.m_technology = NodeUtils::TechnologyType::STREAMBRIDGE;
            } else if (device_type == "TCP") {
                options.m_technology = NodeUtils::TechnologyType::TCP;
            } else {
                std::cout << "== Invalid device type." << '\n';
                exit(1);
            }
        }
    } else {
        std::cout << "== You must specify a device type." << '\n';
        exit(1);
    }

    // Parse node's port to open
    itr = find (args.begin(), args.end(), "-port");
    if (itr != args.end()) {
        ++itr;
        if (*itr == "" || itr->find("-") != std::string::npos) {
            std::cout << "== You must specify a port to open." << '\n';
            exit(1);
        } else {
            options.m_port = *itr;
        }
    } else {
        std::cout << "== You must specify a port to open." << '\n';
        exit(1);
    }

    // Parse Client specific options
    if (options.m_operation == NodeUtils::DeviceOperation::BRANCH || options.m_operation == NodeUtils::DeviceOperation::LEAF) {
        // Parse server peer's address
        itr = find (args.begin(), args.end(), "-peer");
        if (itr != args.end()) {
            ++itr;
            if (*itr == "" || itr->find("-") != std::string::npos) {
                std::cout << "== You must specify a peer address." << '\n';
                exit(1);
            } else {
                options.m_peerAddress = *itr;
            }
        } else {
            std::cout << "== You must specify a peer address." << '\n';
            exit(1);
        }

        // Parse server peer's port
        itr = find (args.begin(), args.end(), "-pp");
        if (itr != args.end()) {
            ++itr;
            if (*itr == "" || itr->find("-") != std::string::npos) {
                std::cout << "== You must specify the peer's port." << '\n';
                exit(1);
            } else {
                options.m_peerPort = *itr;
            }
        }
        else {
            std::cout << "== You must specify the peer's port." << '\n';
            exit(1);
        }

    }

}

//------------------------------------------------------------------------------------------------

void CreateTcpSocket() {
    NodeUtils::TOptions tcpSetup;
    tcpSetup.m_technology = NodeUtils::TechnologyType::TCP;
    tcpSetup.m_port = "3001";
    tcpSetup.m_operation = NodeUtils::DeviceOperation::ROOT;
    std::shared_ptr<CConnection> const conn = Connection::Factory(tcpSetup);
    while (true) {
    	std::optional<std::string> const received = conn->Receive(1);
    	if (received->length() > 2) {
    	    conn->Send("THIS IS A MESSAGE");
    	}
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
}

//------------------------------------------------------------------------------------------------

void CreateTcpConnection() {
    NodeUtils::TOptions tcpSetup;
    tcpSetup.m_technology = NodeUtils::TechnologyType::TCP;
    tcpSetup.m_peerAddress = "127.0.0.1";
    tcpSetup.m_peerPort = "3001";
    tcpSetup.m_operation = NodeUtils::DeviceOperation::LEAF;
    std::shared_ptr<CConnection> conn = Connection::Factory(tcpSetup);
    conn->Send("THIS IS A MESSAGE");
    conn->Receive(1);
}

//------------------------------------------------------------------------------------------------

void CreateStreamBridgeSocket() {
    NodeUtils::TOptions streambridgeSetup;
    streambridgeSetup.m_technology = NodeUtils::TechnologyType::STREAMBRIDGE;
    streambridgeSetup.m_port = "3001";
    streambridgeSetup.m_operation = NodeUtils::DeviceOperation::ROOT;
    Connection::Factory(streambridgeSetup);
}

//------------------------------------------------------------------------------------------------

void CreateStreamBridgeSendQuery() {
    NodeUtils::TOptions streambridgeSetup;
    streambridgeSetup.m_technology = NodeUtils::TechnologyType::STREAMBRIDGE;
    //streambridgeSetup.m_technology = TechnologyType::TCP;
    streambridgeSetup.m_port = "3001";
    streambridgeSetup.m_operation = NodeUtils::DeviceOperation::ROOT;
    std::shared_ptr<CConnection> conn = Connection::Factory(streambridgeSetup);
    std::optional<std::string> resp = conn->Receive(1);
    std::cout << "Received: " << resp.value() << "\n";

    CNode node(streambridgeSetup);
    std::shared_ptr<CState> state = std::make_shared<CState>(streambridgeSetup);
    std::unique_ptr<Command::CHandler> c = Command::Factory(NodeUtils::CommandType::QUERY, node, state);

    NodeUtils::CommandType const command = NodeUtils::CommandType::QUERY;
    std::int8_t const phase = 0;
    std::string const nodeId = "00-00-00-00-00";
    std::string const data = "INITIAL COMMAND!";
    std::uint32_t const nonce = 998;
    CMessage message(nodeId, "", command, phase, data, nonce);

    conn->Send(message);

    resp = conn->Receive(1);
    std::cout << "Received: " << resp.value() << "\n";

    CMessage const responseMessage(resp.value());
    std::cout << "Message Content: " << responseMessage.GetData() << '\n';

    c->HandleMessage(responseMessage);
}

//------------------------------------------------------------------------------------------------

std::int32_t main(std::int32_t argc, char** argv) {
    ParseArguments(argc, argv);

    if (options.m_runTests == true) {
        RunTests();
        exit(0);
    }

    std::cout << "\n== Welcome to the Brypt Network\n";
    std::cout << "Main process PID: " << getpid() << "\n";

    CNode alpha(options);
    alpha.Startup();

    return 0;
}

//------------------------------------------------------------------------------------------------
