#include "node.hpp"

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
    std::cout << "\n== Setting up Brypt Node\n";

    unsigned int port_num = std::stoi(options.port);

    this->id = options.id;
    this->operation = options.operation;
    this->coordinator_technology = options.technology;
    this->next_full_port = port_num + PORT_GAP;
    this->notifier = new Notifier(std::to_string(port_num + 1));

    options.addr = get_local_address();

    switch (options.operation) {
        case ROOT: {
            TechnologyType technology = determineBestConnectionType();
            technology = DIRECT_TYPE; //TEMPORARY

            if (technology == NONE) {
                std::cout << "== [Node] No technology types oopsies\n";
                exit(1);
            }

            this->control = new Control(technology, &this->id, options.port);

            break;
        }
        case BRANCH: {

            break;
        }
        case LEAF: {
            this->coordinator_addr = options.peer_addr;
            initial_contact(&options);  // Contact coordinator peer to get connection port
            break;
        }
    }
    //this->connections.push_back(ConnectionFactory(options.technology, &options));
}

/* **************************************************************************
** Function:
** Description:
** *************************************************************************/
Connection * Node::setup_wifi_connection(std::string peer_id, std::string port) {
    Options opts;
    opts.technology = DIRECT_TYPE;
    opts.operation = ROOT;
    opts.port = port;
    opts.is_control = false;
    opts.peer_name = peer_id;
    this->message_queue.push_pipe("./tmp/" + opts.peer_name + ".pipe");

    Connection * connection = ConnectionFactory(DIRECT_TYPE, &opts);

    return connection;
}

// Communication Functions
/* **************************************************************************
** Function:
** Description:
** *************************************************************************/
void Node::initial_contact(Options * opts) {
    Connection * connection;
    std::string response;

    std::cout << "Setting up initial contact\n";
    std::cout << "Connecting with technology: " << opts->technology << " and on addr:port: " << opts->peer_addr << ":" << opts->peer_port << "\n";

    opts->is_control = true;
    connection = ConnectionFactory(opts->technology, opts);

    std::cout << "== [Node] Sending coordinator acknowledgement\n";
    connection->send("\x06");   // Send initial ACK byte to peer
    response = connection->recv();  // Expect ACK back from peer
    std::cout << "== [Node] Received: " << (int)response.c_str()[0] << "\n";

    std::cout << "== [Node] Sending SYN byte\n";
    connection->send("\x16");   // Send SYN byte to peer
    response = connection->recv();  // Expect new connection port from peer

    Message port_message(response);
    this->coordinator_id = port_message.get_node_id();
    this->coordinator_port = port_message.get_data();
    std::cout << "== [Node] Port received: " << this->coordinator_port << "\n";

    std::cout << "== [Node] Sending node information\n";
    Message info_message(this->id, INFORMATION_TYPE, 1, "Node Information", 0);
    connection->send(&info_message);    // Send node information to peer

    response = connection->recv();  // Expect EOT back from peer
    std::cout << "== [Node] Received: " << (int)response.c_str()[0] << "\n";
    std::cout << "== [Node] Connection sequence completed. Connecting to new endpoint.\n";

    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    connection->shutdown(); // Shutdown initial connection
}

/* **************************************************************************
** Function:
** Description:
** *************************************************************************/
void Node::join_coordinator() {
    Connection * connection;

    std::cout << "Joining coordinator cluster with full connection\n";
    std::cout << "Connecting with technology: " << this->coordinator_technology;
    std::cout << " and on addr:port: " << this->coordinator_addr << ":" << this->coordinator_port << "\n";

    Options options;

    options.port = "";
    options.technology = this->coordinator_technology;
    options.peer_name = this->coordinator_id;
    options.peer_addr = this->coordinator_addr;
    options.peer_port = this->coordinator_port;
    options.operation = LEAF;
    options.is_control = true;

    connection = ConnectionFactory(this->coordinator_technology, &options);
    this->connections.push_back(connection);
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
void Node::handle_control_request(std::string message) {
    std::cout << "== [Node] Handling request from control socket\n";

    if (message == "") {
        std::cout << "== [Node] No request to handle\n";
        return;
    }

    try {

        Message request(message);

        switch (request.get_command()) {
            case INFORMATION_TYPE: {
                std::cout << "== [Node] Setting up full connection\n";
                this->next_full_port++;
                std::string full_port = std::to_string(this->next_full_port);

                Connection *full = this->setup_wifi_connection(request.get_node_id(), full_port);
                this->connections.push_back(full);
                if (full->get_worker_status()) {
                    std::cout << "== [Node] Connection worker thread is ready" << '\n';
                }

                std::cout << "== [Node] New connection pushed back\n";
                this->control->send("\x04");
                break;
            }
            default: {
                this->control->send("\x15");
                break;
            }
        }

    } catch (...) {
        std::cout << "== [Node] Control message failed to unpack.\n";
        return;
    }

}

/* **************************************************************************
** Function:
** Description:
** *************************************************************************/
void Node::handle_notification(std::string message) {
    std::cout << "== [Node] Handling notification from coordinator\n";

    if (message == "") {
        std::cout << "== [Node] No notification to handle\n";
        return;
    }


}

/* **************************************************************************
** Function:
** Description:
** *************************************************************************/
void Node::handle_queue_request(Message * message) {
    std::cout << "== [Node] Handling queue request from connection thread\n";

    if (message->get_command() == NULL_CMD) {
        std::cout << "== [Node] No request to handle\n";
        return;
    }

    try {

        switch (message->get_command()) {
            case QUERY_TYPE: {
                std::cout << "== [Node] Recieved " << message->get_data() << " from " << message->get_node_id() << " thread" << '\n';
                break;
            }
            default: {
                // this->control->send("\x15");
                break;
            }
        }

    } catch (...) {
        std::cout << "== [Node] Queue message failed to unpack.\n";
        return;
    }

}


// Run Functions
/* **************************************************************************
** Function:
** Description:
** *************************************************************************/
void Node::listen(){
    std::cout << "== Brypt Node is listening\n";
    do {
        std::string control_request = "";
        std::string notification = "";
        Message queue_request;

        control_request = this->control->recv();
        this->handle_control_request(control_request);

        notification = this->notifier->recv();
        this->handle_notification(notification);

        this->message_queue.check_pipes();

        queue_request = message_queue.pop_next_message();
        this->handle_queue_request(&queue_request);

        std::cout << '\n';

        std::this_thread::sleep_for(std::chrono::seconds(2));
    } while (true);
}

/* **************************************************************************
** Function:
** Description:
** *************************************************************************/
void Node::connect(){
    std::cout << "== Brypt Node is connecting\n";
    this->join_coordinator();
    unsigned int run = 0;

    do {
        std::string response = "";
        std::string notification = "";

        // Handle Notification
        notification = this->notifier->recv();
        this->handle_notification(notification);

        // Send information to coordinator based on notification
        // Send Request to the server
        Message message(this->id, QUERY_TYPE, 0, std::to_string(run) + " Here is some work!", run);
        this->connections[0]->send(&message);

        // Recieve Response from the server
        response = this->connections[0]->recv();
        std::cout << "== [Node] Recieved: " << response << '\n';

        run++;
        std::this_thread::sleep_for(std::chrono::milliseconds(3750));
    } while (true);
}

/* **************************************************************************
** Function:
** Description:
** *************************************************************************/
void Node::startup() {
    std::cout << "\n== Starting up Brypt Node\n";

    switch (this->operation) {
        case ROOT: {
            // Move to thread?
            this->listen();
            break;
        }
        case BRANCH: {
            // Listen in thread?
            // Connect in another thread?
            // Bridge threads to receive upstream notifications and then pass down to own leafs
            // + pass aggregated messages to connect thread to respond with
            break;
        }
        case LEAF: {
            // Move to thread?
            this->connect();
            break;
        }
    }
}
