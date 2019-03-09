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
    this->id = options.id;
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
            initial_contact(&options);
            break;
        }
    }
    //this->connections.push_back(ConnectionFactory(options.technology, &options));
}

/* **************************************************************************
** Function:
** Description:
** *************************************************************************/
Connection * Node::setup_wifi_connection(std::string port) {
    Options opts;
    opts.technology = DIRECT_TYPE;
    opts.operation = SERVER;
    opts.port = port;
    opts.is_control = false;
    Connection * connection = ConnectionFactory(DIRECT_TYPE, &opts);
    return connection;
}

// Communication Functions
/* **************************************************************************
** Function:
** Description:
** *************************************************************************/
void Node::connect() {
    std::cout << "Connecting..." << '\n';
}

/* **************************************************************************
** Function:
** Description:
** *************************************************************************/
void Node::initial_contact(Options * opts) {
    Connection * connection;
    std::string response;

    std::cout << "Setting up initial contact\n";
    std::cout << "Connecting with technology: " << opts->technology << " and on IP:port: " << opts->peer_IP << ":" << opts->peer_port << "\n";

    opts->is_control = true;
    connection = ConnectionFactory(opts->technology, opts);

    connection->send("\x06");
    response = connection->recv();
    std::cout << "== [Node] Received: " << (int)response.c_str()[0] << "\n";


    connection->send("\x16");
    response = connection->recv();
    std::cout << "== [Node] Received: " << (int)response.c_str()[0] << "\n";

    Message port_message(response);
    opts->peer_port = port_message.get_data();
    std::cout << "== [Node] Port received: " << opts->peer_port << "\n";

    Message info_message(this->id, INFORMATION_TYPE, 1, "Device Information", 0);
    connection->send(&info_message);

    response = connection->recv();
    std::cout << "== [Node] Connection sequence completed. Connecting to new endpoint.\n";

    connection->shutdown();
}

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

// Request Handlers
/* **************************************************************************
** Function:
** Description:
** *************************************************************************/
void Node::handle_control_request(std::string request) {
    if (request == "WIFI") {
        std::string full_port = "";
        std::string device_info = "";

        this->next_conn_port++;
        full_port = std::to_string(this->next_conn_port);

        std::cout << "== [Node] Sending port: " << full_port << "\n";
        Message port_message(this->id, CONNECT_TYPE, 0, full_port, 0);
        this->control->send(&port_message);

        device_info = this->control->recv();
        std::cout << "== [Node] Received: " << device_info << "\n";

        this->control->send("\x04");

        sleep(2);

        std::cout << "== [Node] Setting up full connection\n";

        Connection *full = this->setup_wifi_connection(full_port);
        this->connections.push_back(full);

        std::cout << "== [Node] New connection pushed back\n";
    }
}


// Communication Functions
/* **************************************************************************
** Function:
** Description:
** *************************************************************************/
void Node::listen(){
    std::string control_request = "";
    std::string queued_request = "";

    control_request = this->control->recv();
    this->handle_control_request(control_request);

}
