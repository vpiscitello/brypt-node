#include "node.hpp"

// Constructors and Deconstructors
/* **************************************************************************
** Function:
** Description:
** *************************************************************************/
Node::Node() {
    this->commands.push_back( CommandFactory(INFORMATION_TYPE, this, &state) );
    this->commands.push_back( CommandFactory(QUERY_TYPE, this, &state) );
    this->commands.push_back( CommandFactory(ELECTION_TYPE, this, &state) );
    this->commands.push_back( CommandFactory(TRANSFORM_TYPE, this, &state) );
    this->commands.push_back( CommandFactory(CONNECT_TYPE, this, &state) );
}

/* **************************************************************************
** Function:
** Description:
** *************************************************************************/
Node::~Node() {
    if (this->control != NULL) {
        free(this->control);
    }
    if (this->notifier != NULL) {
        free(this->notifier);
    }
    if (this->watcher != NULL) {
        free(this->watcher);
    }
    std::vector<Connection *>::iterator conn_it;
    for(conn_it = this->connections.begin(); conn_it != this->connections.end(); conn_it++) {
        if ((*conn_it) != NULL) {
            free(*conn_it);
        }
    }
    std::vector<Command *>::iterator cmd_it;
    for(cmd_it = this->commands.begin(); cmd_it != this->commands.end(); cmd_it++) {
        if ((*cmd_it) != NULL) {
            free(*cmd_it);
        }
    }
}

/* **************************************************************************
** Function:
** Description:
** *************************************************************************/
class Control * Node::get_control() {
    return this->control;
}

/* **************************************************************************
** Function:
** Description:
** *************************************************************************/
class Notifier * Node::get_notifier() {
    return this->notifier;
}

/* **************************************************************************
** Function:
** Description:
** *************************************************************************/
std::vector<class Connection *> * Node::get_connections() {
    return &this->connections;
}

/* **************************************************************************
** Function:
** Description:
** *************************************************************************/
class Connection * Node::get_connection(unsigned int index) {
    return this->connections.at(index);
}

/* **************************************************************************
** Function:
** Description:
** *************************************************************************/
class MessageQueue * Node::get_message_queue() {
    return &this->message_queue;
}

/* **************************************************************************
** Function:
** Description:
** *************************************************************************/
class AwaitContainer * Node::get_awaiting() {
    return &this->awaiting;
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
float Node::determine_node_power(){
    float value = 0.0;

    return value;
}

/* **************************************************************************
** Function:
** Description:
** *************************************************************************/
TechnologyType Node::determine_best_connection_type(){
    int best_comm = 4;

    for (int i = 0; i < (int)this->state.self.available_technologies.size(); i++) {
    	if (this->state.self.available_technologies[i] == DIRECT_TYPE || this->state.self.available_technologies[i] == STREAMBRIDGE_TYPE || this->state.self.available_technologies[i] == TCP_TYPE) {
    	    return state.self.available_technologies[i];
    	} else if (this->state.self.available_technologies[i] == BLE_TYPE) {
    	    if (best_comm > 1) {
    		    best_comm = 1;
    	    }
    	} else if (this->state.self.available_technologies[i] == LORA_TYPE) {
    	    if (best_comm > 2) {
    		    best_comm = 2;
    	    }
    	} else {
    	    if (best_comm > 4) {
    		    best_comm = 4;
    	    }
    	}
    }

    return static_cast<TechnologyType>(best_comm);
}


/* **************************************************************************
** Function:
** Description:
** *************************************************************************/
bool Node::has_communication_type(TechnologyType t){
    for (int i = 0; i < (int) this->state.self.available_technologies.size(); i++) {
	if (this->state.self.available_technologies[i] == t) {
	    return true;
	}
    }
    return false;
}


// Election Functions
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

    this->state.self.id = options.id;
    this->state.self.operation = options.operation;
    this->state.coordinator.technology = options.technology;
    this->state.self.port = options.port;
    this->state.self.next_full_port = port_num + PORT_GAP;
    this->notifier = new Notifier(&this->state, (class Node *)this, std::to_string(port_num + 1));
    this->watcher = new PeerWatcher(this, &this->state);

    options.addr = get_local_address();

    switch (options.operation) {
        case ROOT: {
	    // Currently a ROOT node only has one control socket so there is only one communication type for it. In the future, coordinators may have control sockets on both communication types and thus this functionality will change.
	    TechnologyType technology = NO_TECH;
	    this->state.self.available_technologies.push_back(options.technology);// TEMPORARY
	    if (has_communication_type(BLE_TYPE)) {
		technology = BLE_TYPE;
	    } else if (has_communication_type(LORA_TYPE)) {
		technology = LORA_TYPE;
	    } else if (has_communication_type(DIRECT_TYPE) || has_communication_type(STREAMBRIDGE_TYPE) || has_communication_type(TCP_TYPE)){
		technology = TCP_TYPE;
	    }

            if (technology == NO_TECH) {
		printo("No technology types oopsies", NODE_P);
                exit(1);
            }

	    //technology = STREAMBRIDGE_TYPE; // Temporary

            this->control = new Control(technology, &this->state.self);

            break;
        }
        case BRANCH: {

            break;
        }
        case LEAF: {
	    this->state.self.available_technologies.push_back(options.technology);// TEMPORARY
            TechnologyType technology = determine_best_connection_type();

            if (technology == NO_TECH) {
		printo("No technology types oopsies", NODE_P);
                exit(1);
            }

            this->state.coordinator.addr = options.peer_addr;
            this->state.coordinator.publisher_port = std::to_string(std::stoi(options.peer_port) + 1);
            initial_contact(&options);  // Contact coordinator peer to get connection port

            break;
        }
        case NO_OPER: {
	    printo("ERROR DEVICE OPERATION NEEDED", NODE_P);
            exit(0);

            break;
        }
    }
    //this->connections.push_back(ConnectionFactory(options.technology, &options));
}

/* **************************************************************************
** Function:
** Description:
** *************************************************************************/
Connection * Node::setup_full_connection(std::string peer_id, std::string port, TechnologyType comm_tech) {
    Options opts;
    if (comm_tech != DIRECT_TYPE) {
	comm_tech = TCP_TYPE;
    }
    opts.technology = comm_tech;
    opts.operation = ROOT;
    opts.port = port;
    opts.is_control = false;
    opts.peer_name = peer_id;
    this->message_queue.push_pipe("./tmp/" + opts.peer_name + ".pipe");

    Connection * connection = ConnectionFactory(comm_tech, &opts);

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
    TechnologyType initial_contact_technology;

    if (opts->technology == LORA_TYPE) {
	initial_contact_technology = LORA_TYPE;
    } else if (opts->technology == BLE_TYPE) {
	initial_contact_technology = BLE_TYPE;
    } else {
	initial_contact_technology = TCP_TYPE;
    }
 
    printo("Setting up initial contact with coordinator", NODE_P);
    printo("Connecting with initial contact technology: " + std::to_string((int)initial_contact_technology) +  " and on addr:port: " +  opts->peer_addr + ":" + opts->peer_port, NODE_P);

    opts->is_control = true;
    connection = ConnectionFactory(initial_contact_technology, opts);

    printo("Sending coordinator acknowledgement", NODE_P);
    connection->send("\x06");   // Send initial ACK byte to peer
    response = connection->recv(0);  // Expect ACK back from peer
    printo("Received: " + std::to_string((int)response.c_str()[0]) + "\n", NODE_P);

    // Send communication technology preferred
    printo("Sending preferred contact technology", NODE_P);
    connection->send(std::to_string(opts->technology).c_str());
    response = connection->recv(0);  // Expect new connection port from peer or something related to LoRa/Bluetooth

    Message port_message(response);
    this->state.coordinator.id = port_message.get_source_id();
    this->state.coordinator.request_port = port_message.get_data();
    printo("Port received: " + this->state.coordinator.request_port, NODE_P);

    printo("Sending node information", NODE_P);
    Message info_message(this->state.self.id, this->state.coordinator.id, CONNECT_TYPE, 1, std::to_string(opts->technology).c_str(), 0);
    connection->send(&info_message);    // Send node information to peer

    response = connection->recv(0);  // Expect EOT back from peer
    printo("Received: " + std::to_string( (int)response.c_str()[0] ), NODE_P);
    printo("Connection sequence completed. Connecting to new endpoint", NODE_P);

    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    printo("Connection sequence completed. Shutting down initial connection", NODE_P);
    connection->shutdown(); // Shutdown initial connection
}

/* **************************************************************************
** Function:
** Description:
** *************************************************************************/
void Node::join_coordinator() {
    Connection * connection;

    printo("Joining coordinator cluster with full connection", NODE_P);
    printo("Connecting with technology: " + std::to_string((int)this->state.coordinator.technology) + " and on addr:port: " + this->state.coordinator.addr + ":" + this->state.coordinator.request_port, NODE_P);

    this->state.network.known_nodes++;

    this->notifier->connect(this->state.coordinator.addr, this->state.coordinator.publisher_port);

    Options options;

    options.port = "";
    options.technology = this->state.coordinator.technology;
    options.peer_name = this->state.coordinator.id;
    options.peer_addr = this->state.coordinator.addr;
    options.peer_port = this->state.coordinator.request_port;
    options.operation = LEAF;
    options.is_control = true;

    connection = ConnectionFactory(this->state.coordinator.technology, &options);
    this->connections.push_back(connection);
}

/* **************************************************************************
** Function:
** Description:
** *************************************************************************/
bool Node::contact_authority(){
    bool success = false;

    return success;
}

/* **************************************************************************
** Function:
** Description:
** *************************************************************************/
bool Node::notify_address_change(){
    bool success = false;

    return success;
}

/* **************************************************************************
** Function:
** Description:
** *************************************************************************/
void Node::notify_connection(std::string id) {
    std::vector<Connection *>::iterator it;
    for (it = this->connections.begin(); it != this->connections.end(); it++) {
        if ((*it)->get_peer_name() == id) {
            (*it)->response_ready(id);
        }
    }
}

// Request Handlers
/* **************************************************************************
** Function:
** Description:
** *************************************************************************/
void Node::handle_control_request(std::string message) {
    printo("Handling request from control socket", NODE_P);

    if (message == "") {
        printo("No request to handle", NODE_P);
        return;
    }

    try {

        Message request(message);

        this->commands[request.get_command()]->handle_message(&request);

    } catch (...) {
	printo("Control message failed to unpack", NODE_P);
        return;
    }

}

/* **************************************************************************
** Function:
** Description:
** *************************************************************************/
void Node::handle_notification(std::string message) {
    printo("Handling notification from coordinator", NODE_P);

    if (message == "") {
        printo("No notification to handle", NODE_P);
        return;
    }

    try {
        std::string notice_filter = "";
        std::string raw_notification = "";
        std::size_t filter_found = message.find(":");
        if (filter_found != std::string::npos && filter_found < 16) {
            notice_filter = message.substr(0, filter_found);
            raw_notification = message.substr(filter_found+1);
        }

        Message notification(raw_notification);

        this->commands[notification.get_command()]->handle_message(&notification);

    } catch (...) {
        printo("Notice message failed to unpack", NODE_P);
        return;
    }


}

/* **************************************************************************
** Function:
** Description:
** *************************************************************************/
void Node::handle_queue_request(Message * message) {
    printo("Handling queue request from connection thread", NODE_P);

    if (message->get_command() == NO_CMD) {
        printo("No command to handle", NODE_P);
        return;
    }

    this->commands[message->get_command()]->handle_message(message);

}

/* **************************************************************************
** Function:
** Description:
** *************************************************************************/
void Node::handle_fulfilled() {
    printo("Sending off fulfilled requests", NODE_P);

    if (this->awaiting.empty()) {
        printo("No awaiting requests", NODE_P);
        return;
    }

    printo("Fulfulled requests:", NODE_P);
    std::vector<class Message> responses = this->awaiting.get_fulfilled();


    for (auto it = responses.begin(); it != responses.end(); it++) {
        this->message_queue.add_message((*it).get_destination_id(), *it);
    }

    this->message_queue.push_pipes();

    for (auto it = responses.begin(); it != responses.end(); it++) {
        this->notify_connection((*it).get_destination_id());
    }

}



// Run Functions
/* **************************************************************************
** Function:
** Description:
** *************************************************************************/
void Node::listen(){
    printo("Brypt Node is listening", NODE_P);
    unsigned int run = 1;
    do {
        std::string control_request = "";
        std::string notification = "";
        Message queue_request;

        control_request = this->control->recv();
	if (control_request != "") {
	    this->handle_control_request(control_request);
	    this->control->close_current_connection();
	}

        notification = this->notifier->recv();
        this->handle_notification(notification);

        this->message_queue.check_pipes();

        queue_request = message_queue.pop_next_message();
        this->handle_queue_request(&queue_request);

        this->handle_fulfilled();

        std::cout << '\n';

        // SIMULATE CLIENT REQUEST
        /* if (run % 10 == 0) {
            std::cout << "== [Node] Simulating client sensor Information request" << '\n';
            Message message("0xFFFFFFFF", this->state.self.id, INFORMATION_TYPE, 0, "Request for Network Information.", run);
            this->commands[message.get_command()]->handle_message(&message);
        }

        if(run % 10 == 0) {
            std::cout << "== [Node] Simulating client sensor Query request" << '\n';
            Message message("0xFFFFFFFF", this->state.self.id, QUERY_TYPE, 0, "Request for Sensor Readings.", run);
            this->commands[message.get_command()]->handle_message(&message);
        } */

        run++;
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    } while (true);
}

/* **************************************************************************
** Function:
** Description:
** *************************************************************************/
void Node::connect(){
    printo("Brypt Node is connecting", NODE_P);
    this->join_coordinator();
    printo("Joined coordinator", NODE_P);
    unsigned int run = 0;

    do {
        std::string response = "";
        std::string notification = "";

        // Handle Notification
        notification = this->notifier->recv();
        // Send information to coordinator based on notification
        this->handle_notification(notification);

        run++;
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    } while (true);
}

/* **************************************************************************
** Function:
** Description:
** *************************************************************************/
void Node::startup() {
    printo("Starting up Brypt Node", NODE_P);

    srand(time(NULL));

    switch (this->state.self.operation) {
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
        case NO_OPER: {
	    printo("ERROR DEVICE OPERATION NEEDED", NODE_P);
            exit(0);
            break;
        }
    }
}
