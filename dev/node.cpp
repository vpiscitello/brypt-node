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

// Utility Functions
/* **************************************************************************
** Function:
** Description:
** *************************************************************************/
void Node::pack() {

}

/* **************************************************************************
** Function:
** Description:
** *************************************************************************/
void Node::unpack() {

}

/* **************************************************************************
** Function:
** Description:
** *************************************************************************/
long long Node::getCurrentTimestamp() {
    long long timestamp = 0;

    return timestamp;
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
    switch (options.operation) {
        case SERVER:
            this->isRoot = true;
            this->isBranch = true;
            this->isLeaf = false;
            break;
        case CLIENT:
            this->isRoot = false;
            this->isBranch = false;
            this->isLeaf = true;
            break;
    }
    //this->connections.push_back(ConnectionFactory(options.technology, &options));

    // Should this socket be running on clients?
    TechnologyType t = determineBestConnectionType();
    t = DIRECT_TYPE; //TEMPORARY
    std::cout << "Technology type is: " << t << "\n";
    if (t == NONE) {
	std::cout << "No technology types oopsies\n";
	exit(1);
    }
    this->control = new Control(t);
}

// Setup will use the functionality known in the device (from ryan's daemon) what connection to create

// Information Functions
/* **************************************************************************
** Function:
** Description:
** *************************************************************************/
// std::map<std::string, std::string>  Node::getDeviceInformation(){
//     std::map<std::string, std::string> information;
//
//     return information;
// }


// Connect Functions
/* **************************************************************************
** Function:
** Description:
** *************************************************************************/
void Node::connect(){
    std::cout << "Connecting..." << '\n';
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
    // this starts the initial control socket, when it receives input, it sends an interrupt upward to this node and requests startup, then it restarts when that is complete.
    do {
	std::string req_type = this->control->listen();
	if (req_type == "WIFI") {
	    // Create a connection
	    std::string wifi_port = "3010";
	    // Push it back on our connections
	    Options new_wifi_device;
	    new_wifi_device.technology = DIRECT_TYPE;
	    new_wifi_device.operation = CLIENT;
	    new_wifi_device.port = "3001";
	    new_wifi_device.peer_IP = "localhost";
	    new_wifi_device.peer_port = wifi_port;
	    //this->connections.push_back(ConnectionFactory(DIRECT_TYPE, &new_wifi_device));

	    this->control->send(wifi_port);
	    this->control->listen();
	}
    } while (true);
}

//transmit should loop over all neighbor nodes send a request to connect, receive a status, send a query, receive a query, send an EOM
/* **************************************************************************
** Function:
** Description:
** *************************************************************************/
bool Node::transmit(std::string message){
    bool success = false;
    //this should send over all neighbor nodes
    //this->control_conn->send(message);
    //for (int i = 0; i < (int)this->connections.size(); i++) {
    //    
    //}

    return success;
}

//receive may not be needed
/* **************************************************************************
** Function:
** Description:
** *************************************************************************/
std::string Node::receive(int message_size){
    std::string message = "ERROR";
    //use nonblocking check to call this for all connections and see if there is information, if so receive a whole block and return it
    //this->control_conn->serve(message_size);

    return message;
}

//if we have vector of connections, we essentially have a vector of threads since each connection is on a thread.

//poor nodes will only have one receive from a parent coordinator node
