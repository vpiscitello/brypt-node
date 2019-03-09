#include "node.hpp"
#include "control.hpp"

// Constructors and Deconstructors
/* **************************************************************************
** Function:
** Description:
** *************************************************************************/
Node::Node() {

}

/* **************************************************************************
** Function:
** Description:
** *************************************************************************/
Node::~Node() {

}

// Communication Functions
/* **************************************************************************
** Function:
** Description:
** *************************************************************************/
bool Node::contactAuthority(){
    bool success = false;

    return success;
}

/* **************************************************************************
** Function:
** Description:
** *************************************************************************/
bool Node::notifyAddressChange(){
    bool success = false;

    return success;
}

/* **************************************************************************
** Function:
** Description:
** *************************************************************************/
TechnologyType Node::determineBestConnectionType(){
    int best_comm = 4;

    for (int i = 0; i < (int)this->communicationTechnologies.size(); i++) {
    	if (communicationTechnologies[i] == DIRECT_TYPE) {
    	    return DIRECT_TYPE;
    	} else if (communicationTechnologies[i] == BLE_TYPE) {
    	    if (best_comm > 1) {
    		    best_comm = 1;
    	    }
    	} else if (communicationTechnologies[i] == LORA_TYPE) {
    	    if (best_comm > 2) {
    		    best_comm = 2;
    	    }
    	} else {
    	    if (best_comm > 3) {
    		    best_comm = 3;
    	    }
    	}
    }

    return static_cast<TechnologyType>(best_comm);
}

/* **************************************************************************
** Function:
** Description:
** *************************************************************************/
//receive a connection and determine what comm tech it is using.
int Node::determineConnectionMethod(){
    int method = 0;

    return method;
}


// Election Functions
/* **************************************************************************
** Function:
** Description:
** *************************************************************************/
bool Node::vote(){
    bool success = false;

    return success;
}

/* **************************************************************************
** Function:
** Description:
** *************************************************************************/
bool Node::election(){
    bool success = false;

    return success;
}

/* **************************************************************************
** Function:
** Description:
** *************************************************************************/
float Node::determineNodePower(){
    float value = 0.0;

    return value;
}

/* **************************************************************************
** Function:
** Description:
** *************************************************************************/
bool Node::transform(){
    bool success = false;

    return success;
}

// Setup Functions
/* **************************************************************************
** Function:
** Description:
** *************************************************************************/
void Node::setup(Options options){
    options.IP = get_local_address();
    switch (options.operation) {
        case SERVER: {
            this->isRoot = true;
            this->isBranch = true;
            this->isLeaf = false;

            TechnologyType t = determineBestConnectionType();
            t = DIRECT_TYPE; //TEMPORARY

            std::cout << "Technology type is: " << t << "\n";
            if (t == NONE) {
                std::cout << "No technology types oopsies\n";
                exit(1);
            }

            this->control = new Control(t, options.port);

            break;
        }
        case CLIENT: {
            this->isRoot = false;
            this->isBranch = false;
            this->isLeaf = true;

            //Some sort of setup to determine the host and port to connect to
            setup_initial_contact(&options);
            break;
        }
    }
    //this->connections.push_back(ConnectionFactory(options.technology, &options));
}

// Setup will use the functionality known in the device (from ryan's daemon) what connection to create

// Connect Functions
/* **************************************************************************
** Function:
** Description:
** *************************************************************************/
void Node::connect(){
    std::cout << "Connecting..." << '\n';
}

void Node::setup_initial_contact(Options * opts) {
    Connection * connection;
    std::string response;

    std::cout << "Setting up initial contact\n";
    std::cout << "Connecting with technology: " << opts->technology << " and on IP:port: " << opts->peer_IP << ":" << opts->peer_port << "\n";

    opts->is_control = true;
    connection = ConnectionFactory(opts->technology, opts);

    connection->send("\x06");
    response = connection->recv();
    std::cout << "== [CLIENT SETUP] Received: " << response << "\n";


    connection->send("\x16");
    response = connection->recv();
    std::cout << "== [CLIENT SETUP] Received: " << response << "\n";

    Message port_msg(response);
    std::string new_port = port_msg.get_data();
    std::cout << "== [CLIENT SETUP] Port received: " << port_msg.get_data() << "\n";

    // data = "EOF";
    // Message message3(node_id, command, phase, data, nonce);
    // connection->send(&message3);
    //
    // response = "";
    // while (response == "") {
    //     response = connection->recv();
    // }
    // std::cout << "== [CLIENT SETUP] Received: " << response << "\n";

    connection->shutdown();
    opts->peer_port = new_port;

    // sleep(10);
    //
    // connection = ConnectionFactory(opts->technology, opts);
    // int msg_num = 0;
    // while (msg_num < 3) {
    //
	// data = "HELLO" + std::to_string(msg_num);
	// Message repeat_msg(node_id, command, phase, data, nonce);
    //     connection->send(&repeat_msg);
    //
    //     std::string response = "";
    //     while (response == "") {
    //         response = connection->recv();
    //     }
    //     std::cout << "== [CLIENT SETUP] Received: " << response << "\n";
    //     msg_num++;
    //     sleep(2);
    // }
    //
    // data = "SHUTDOWN";
    // Message repeat_msg(node_id, command, phase, data, nonce);
    // connection->send(&repeat_msg);
    //
    // response = "";
    // while (response == "") {
    //     response = connection->recv();
    // }
    // std::cout << "== [CLIENT SETUP] Received: " << response << "\n";
    //
    // connection->shutdown();
}

/* **************************************************************************
** Function:
** Description:
** *************************************************************************/
std::string Node::get_local_address(){
    std::string ip = "";

    struct ifaddrs * ifAddrStruct=NULL;
    struct ifaddrs * ifa=NULL;
    void * tmpAddrPtr=NULL;

    getifaddrs(&ifAddrStruct);

    for (ifa = ifAddrStruct; ifa != NULL; ifa = ifa->ifa_next) {
        if (!ifa->ifa_addr) {
            continue;
        }

        if (ifa->ifa_addr->sa_family == AF_INET) {
            tmpAddrPtr=&((struct sockaddr_in *)ifa->ifa_addr)->sin_addr;
            char addressBuffer[INET_ADDRSTRLEN];
            inet_ntop(AF_INET, tmpAddrPtr, addressBuffer, INET_ADDRSTRLEN);
            if (std::string(ifa->ifa_name).find("en0") == 0) {
                ip = std::string(addressBuffer);
                break;
            }
        }
    }
    if (ifAddrStruct!=NULL) freeifaddrs(ifAddrStruct);

    return ip;
}

// Communication Functions
/* **************************************************************************
** Function:
** Description:
** *************************************************************************/
void Node::listen(){
    std::string req_type = this->control->listen();

    if (req_type == "WIFI") {
    	this->next_comm_port++;

    	std::string wifi_port = std::to_string(this->next_comm_port);

    	Options new_wifi_device;
    	new_wifi_device.technology = DIRECT_TYPE;
    	new_wifi_device.operation = SERVER;
    	new_wifi_device.port = wifi_port;
    	new_wifi_device.is_control = false;

    	std::cout << "== [Control] Sending port: " << wifi_port << "\n";
    	CommandType command = CONNECT_TYPE;
    	int phase = 0;
    	std::string node_id = "00-00-00-00-00";
    	std::string data = wifi_port;
    	unsigned int nonce = 998;
    	Message message(node_id, command, phase, data, nonce);
    	this->control->send(&message);
    	std::cout << "Sent port.\n";

    	sleep(2);

    	std::cout << "Creating connection\n";
    	Connection * curr_conn = ConnectionFactory(DIRECT_TYPE, &new_wifi_device);
    	std::cout << "My pid is " << getpid() << "\n";
    	this->connections.push_back(curr_conn);
    	std::cout << "Pushed back connection\n";
    }

}
