//------------------------------------------------------------------------------------------------
// File: Test.cpp
// Description:
//------------------------------------------------------------------------------------------------
#include "Node.hpp"
#include "State.hpp"
#include "Components/Await/Await.hpp"
#include "Components/Command/Handler.hpp"
#include "Components/Connection/Connection.hpp"
#include "Components/MessageQueue/MessageQueue.hpp"
#include "Configuration/Configuration.hpp"
#include "Configuration/ConfigurationManager.hpp"
#include "Utilities/Message.hpp"
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
namespace {
//------------------------------------------------------------------------------------------------
namespace local {
//------------------------------------------------------------------------------------------------

bool RunTests = false;

//------------------------------------------------------------------------------------------------
} // local namespace
//------------------------------------------------------------------------------------------------
} // namespace
//------------------------------------------------------------------------------------------------

Configuration::TConnectionOptions options;

//------------------------------------------------------------------------------------------------

// TODO: Move unit tests into specific unit testing folder
// TODO: Add more unit tests

void ConnectionFactoryTest()
{
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

    // TODO: Something in the TCP connection code is breaking std::cin and to some extent 
    // std::cout after destruction figure out why
    // connections.push_back(Connection::Factory(NodeUtils::TechnologyType::TCP));
    
    connections.push_back(Connection::Factory(NodeUtils::TechnologyType::DIRECT));
    
    // Check the connection type and run a shared function
    for (std::size_t idx = 0; idx < connections.size(); ++idx) {
        connections.at(idx)->whatami();
    }
}

//------------------------------------------------------------------------------------------------

void CommandParseTest()
{
    std::cout << "\n== Testing Command Parsing" << '\n';
    Configuration::TSettings settings;
    settings.connections.emplace_back(options);
    
    // Setup the vector of commands to be used
    CNode node(settings);
    std::shared_ptr<CState> state = std::make_shared<CState>(settings);
    std::vector<std::unique_ptr<Command::CHandler>> commands;
    commands.push_back(Command::Factory(NodeUtils::CommandType::INFORMATION, node, state));
    commands.push_back(Command::Factory(NodeUtils::CommandType::QUERY, node, state));
    commands.push_back(Command::Factory(NodeUtils::CommandType::ELECTION, node, state));
    commands.push_back(Command::Factory(NodeUtils::CommandType::TRANSFORM, node, state));
    commands.push_back(Command::Factory(NodeUtils::CommandType::CONNECT, node, state));

    // Setup a new message to match to a command
    CMessage const message(
        0xFFFFFFFF, 0x00000000,
        NodeUtils::CommandType::ELECTION, 0,
        "This is an election request message.", 9999);

    // Use the message command to handle the message logic
    commands.at(static_cast<std::int32_t>(message.GetCommand()))->HandleMessage(message);
}

//------------------------------------------------------------------------------------------------

void MessageQueueTest()
{
	std::cout << "\n== Testing Message Queue" << '\n';

	CMessageQueue msgQueue;

	std::uint32_t const nodeId = 0xFFFFFFFF;
    CMessage const message(
        nodeId, 0x00000000,
        NodeUtils::CommandType::ELECTION, 0,
        std::string("H\0el\0lo, Wo\0rld\0",16), 9999);

	msgQueue.AddManagedPipe(nodeId);

    std::string const packet = message.GetPack();
	std::cout << packet << std::endl;
	std::fstream file("./tmp/" + std::to_string(nodeId) + ".pipe", std::ios::out | std::ios::binary);
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

void MessageTest()
{
    std::cout << "\n== Testing Messages" << '\n';

    // Setup a new message
    CMessage const message(
        0xABCDEF01, 0xFF00AA99,
        NodeUtils::CommandType::ELECTION, 0,
        "Hello World!", 9999);

    std::string receiveRaw = message.GetPack();      // Get the message as a packed string
    std::cout << "Message Raw: " << receiveRaw << '\n';

    CMessage receiveMessage(receiveRaw);       // Initialize a new message using a received raw string

    Message::VerificationStatus const status = receiveMessage.Verify();      // Verify the message by checking the HMAC
    if (status == Message::VerificationStatus::SUCCESS) {
        std::cout << "Message Verification: Success." << '\n';
    } else {
        std::cout << "Message Verification: Unauthorized!" << '\n';
    }

    std::replace(receiveRaw.begin(), receiveRaw.end(), receiveRaw.at(receiveRaw.size() / 2), '?');
    std::cout << "Tampered Message: " << receiveRaw << '\n';
    CMessage const tamperedMessage(receiveRaw);       // Create a tampered message

    Message::VerificationStatus tamperedStatus = tamperedMessage.Verify();     // Verify the tampered message by checking the HMAC
    if (tamperedStatus == Message::VerificationStatus::SUCCESS) {
        std::cout << "Message Verification: Success." << '\n';
    } else {
        std::cout << "Message Verification: Unauthorized!" << '\n';
    }
    std::cout << std::endl;
}

//------------------------------------------------------------------------------------------------

void AwaitContainerTest()
{
    std::cout << "\n== Testing Await Container" << '\n';

    Await::CObjectContainer awaiting;
    
    CMessage const request(
        0x01234567, 0xFFFFFFFF,
        NodeUtils::CommandType::ELECTION, 0,
        "Hello World!", 9999);
    
    auto const key = awaiting.PushRequest(request, {0xFFFFFFFF, 0xAAAAAAAA, 0xBBBBBBBB});

    CMessage const response(
        0xFFFFFFFF, 0x01234567,
        NodeUtils::CommandType::ELECTION, 0,
        "Hello World!", 9999,
        Message::BoundAwaitId{
            Message::AwaitBinding::DESTINATION, key});

    CMessage const responseA(
        0xAAAAAAAA, 0xFFFFFFFF,
        NodeUtils::CommandType::ELECTION, 0,
        "Hello World!", 9999,
        Message::BoundAwaitId{
            Message::AwaitBinding::DESTINATION, key});

    CMessage const responseB(
        0xBBBBBBBB, 0xFFFFFFFF,
        NodeUtils::CommandType::ELECTION, 0,
        "Hello World!", 9999,
        Message::BoundAwaitId{
            Message::AwaitBinding::DESTINATION, key});

    awaiting.PushResponse(response);
    awaiting.PushResponse(responseA);
    awaiting.PushResponse(responseB);

    auto const fulfilled = awaiting.GetFulfilled();

    for (auto const& message: fulfilled) {
        auto const decrypted = message.Decrypt(message.GetData(), message.GetData().size());
        std::string const str(decrypted->begin(), decrypted->end());
        std::cout << str << std::endl;
    }

}

//------------------------------------------------------------------------------------------------

void ConfigurationManagerTest()
{
    std::cout << "\n== Testing Configuration Manager" << '\n';

    Configuration::CManager manager;
    std::optional<Configuration::TSettings> optSettings = manager.Parse();
    if (optSettings) {
        std::cout << "Configuration settings parsed successfully" << std::endl;
    } else {
        std::cout << "Failed to prase configuration settings" << std::endl;
    }
}

//------------------------------------------------------------------------------------------------

void RunTests() {
    options.technology = NodeUtils::TechnologyType::DIRECT;
    options.operation = NodeUtils::DeviceOperation::ROOT;
    options.interface = "en0";
    options.binding = "3005";
    options.entry_address = "127.0.0.1:9999";

    ConnectionFactoryTest();
    CommandParseTest();
    MessageQueueTest();
    MessageTest();
    AwaitContainerTest();
    ConfigurationManagerTest();
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
        local::RunTests = true;
        return;
    }

    static std::unordered_map<std::string, NodeUtils::DeviceOperation> deviceOperationMap = 
    {
        {"--root", NodeUtils::DeviceOperation::ROOT},
        {"--branch", NodeUtils::DeviceOperation::BRANCH},
        {"--leaf", NodeUtils::DeviceOperation::LEAF}
    };

    for (auto const [key, value] : deviceOperationMap) {
        if (itr = std::find(args.begin(), args.end(), key); itr != args.end()) {
            options.operation = value;
            break;
        }
    }
}

//------------------------------------------------------------------------------------------------

void CreateTcpSocket() {
    Configuration::TConnectionOptions tcpSetup;
    tcpSetup.technology = NodeUtils::TechnologyType::TCP;
    tcpSetup.binding = "3001";
    tcpSetup.operation = NodeUtils::DeviceOperation::ROOT;
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
    Configuration::TConnectionOptions tcpSetup;
    tcpSetup.technology = NodeUtils::TechnologyType::TCP;
    tcpSetup.entry_address = "127.0.0.1:3001";
    tcpSetup.operation = NodeUtils::DeviceOperation::LEAF;
    std::shared_ptr<CConnection> conn = Connection::Factory(tcpSetup);
    conn->Send("THIS IS A MESSAGE");
    conn->Receive(1);
}

//------------------------------------------------------------------------------------------------

void CreateStreamBridgeSocket() {
    Configuration::TConnectionOptions streambridgeSetup;
    streambridgeSetup.technology = NodeUtils::TechnologyType::STREAMBRIDGE;
    streambridgeSetup.binding = "3001";
    streambridgeSetup.operation = NodeUtils::DeviceOperation::ROOT;
    Connection::Factory(streambridgeSetup);
}

//------------------------------------------------------------------------------------------------

void CreateStreamBridgeSendQuery() {
    Configuration::TSettings settings;

    Configuration::TConnectionOptions streambridgeSetup;
    streambridgeSetup.technology = NodeUtils::TechnologyType::STREAMBRIDGE;
    //streambridgeSetup.technology = TechnologyType::TCP;
    streambridgeSetup.binding = "3001";
    streambridgeSetup.operation = NodeUtils::DeviceOperation::ROOT;
    settings.connections.emplace_back(streambridgeSetup);

    std::shared_ptr<CConnection> conn = Connection::Factory(streambridgeSetup);
    std::optional<std::string> resp = conn->Receive(1);
    std::cout << "Received: " << resp.value() << "\n";

    CNode node(settings);
    std::shared_ptr<CState> state = std::make_shared<CState>(settings);
    std::unique_ptr<Command::CHandler> c = Command::Factory(NodeUtils::CommandType::QUERY, node, state);

    std::uint32_t const nodeId = 0xFFFFFFFF;
    NodeUtils::CommandType const command = NodeUtils::CommandType::QUERY;
    std::int8_t const phase = 0;
    std::string const data = "INITIAL COMMAND!";
    std::uint32_t const nonce = 9999;
    CMessage message(nodeId, 0x00000000, command, phase, data, nonce);

    conn->Send(message);

    resp = conn->Receive(1);
    std::cout << "Received: " << resp.value() << "\n";

    CMessage const responseMessage(resp.value());
    // std::cout << "Message Content: " << responseMessage.GetData() << '\n';

    c->HandleMessage(responseMessage);
}

//------------------------------------------------------------------------------------------------

std::int32_t main(std::int32_t argc, char** argv)
{
    ParseArguments(argc, argv);

    if (local::RunTests == true) {
        RunTests();
        exit(0);
    }

    std::cout << "\n== Welcome to the Brypt Network\n";
    std::cout << "Main process PID: " << getpid() << "\n";

    Configuration::CManager configurationManager;
    std::optional<Configuration::TSettings> optSettings = configurationManager.Parse();
    if (optSettings) {
        optSettings->connections[0].operation = options.operation;
        
        CNode alpha(*optSettings);
        alpha.Startup();
    } else {
        std::cout << "Node configuration settings could not be parsed!" << std::endl;
        exit(1);
    }

    return 0;
}

//------------------------------------------------------------------------------------------------
