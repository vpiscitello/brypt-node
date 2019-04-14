#include "command.hpp"

/* **************************************************************************
** Function:
** Description:
** *************************************************************************/
std::string generate_reading() {
    int reading = rand() % ( 74 - 68 ) + 68;

    std::string epoch_str = get_system_timestamp();

    json11::Json reading_json = json11::Json::object({
        { "reading", reading },
        { "timestamp", epoch_str }
    });

    return reading_json.dump();
}

/* **************************************************************************
** Function:
** Description:
** *************************************************************************/
std::string generate_node_info(class Node * node_instance, struct State * state) {
    std::vector<class Connection *> * connections = node_instance->get_connections();
    std::vector<json11::Json::object> nodes_info;

    nodes_info.emplace_back(
        json11::Json::object {
            { "uid", (*state).self.id },
            { "cluster", (*state).self.cluster },
            { "coordinator", (*state).coordinator.id },
            { "neighbor_count", (int)(*state).network.known_nodes },
            { "designation", get_designation((*state).self.operation) },
            { "comm_techs", json11::Json::array { "WiFi" } },
            { "update_timestamp", get_system_timestamp() }
        }
    );

    std::vector<class Connection *>::iterator conn_it = connections->begin();

    for (conn_it; conn_it != connections->end(); conn_it++) {

        std::stringstream epoch_ss;
        std::chrono::seconds seconds;

        seconds = std::chrono::duration_cast<std::chrono::seconds>( (*conn_it)->get_update_clock().time_since_epoch() );
        epoch_ss.clear();
        epoch_ss << seconds.count();

        nodes_info.emplace_back(
            json11::Json::object {
                { "uid", (*conn_it)->get_peer_name() },
                { "cluster", (*state).self.cluster },
                { "coordinator", (*state).self.id },
                { "neighbor_count", 0 },
                { "designation", "node" },
                { "comm_techs", json11::Json::array { (*conn_it)->get_type() } },
                { "update_timestamp", epoch_ss.str() }
            }
        );
    }

    json11::Json nodes_json = json11::Json::array({nodes_info});

    // json11::Json nodes_json = json11::Json::array {
    //     json11::Json::object {
    //         { "uid", "1" },
    //         { "cluster", 0 },
    //         { "coordinator", 1 },
    //         { "neighbor_count", 4 },
    //         { "designation", "root" },
    //         { "comm_techs", json11::Json::array { "WiFi" } },
    //         { "update_timestamp", epoch_str }
    //     },
    //     json11::Json::object {
    //         { "uid", "2" },
    //         { "cluster", 0 },
    //         { "coordinator", 1 },
    //         { "neighbor_count", 4 },
    //         { "designation", "node" },
    //         { "comm_techs", json11::Json::array { "WiFi" } },
    //         { "update_timestamp", epoch_str }
    //     },
    //     json11::Json::object {
    //         { "uid", "3" },
    //         { "cluster", 0 },
    //         { "coordinator", 1 },
    //         { "neighbor_count", 6 },
    //         { "designation", "coordinator" },
    //         { "comm_techs", json11::Json::array { "WiFi", "LoRa" } },
    //         { "update_timestamp", epoch_str }
    //     },
    //     json11::Json::object {
    //         { "uid", "6" },
    //         { "cluster", 0 },
    //         { "coordinator", 0 },
    //         { "neighbor_count", 4 },
    //         { "designation", "node" },
    //         { "comm_techs", json11::Json::array { "WiFi" } },
    //         { "update_timestamp", epoch_str }
    //     }
    // };

    return nodes_json[0].dump();
}

/* **************************************************************************
** Function:
** Description:
** *************************************************************************/
Message Information::handle_message(Message * message) {
    Message response;

    this->whatami();
    this->message = message;
    switch ( message->get_phase() ) {
        case FLOOD_PHASE: {
            this->flood_handler(&(*this->state).self, message, this->node_instance->get_notifier());
            break;
        }
        case RESPOND_PHASE: {
            this->respond_handler();
            break;
        }
        case CLOSE_PHASE: {
            this->close_handler();
            break;
        }
        default: {

            break;
        }
    }
    return response;
}

/* **************************************************************************
** Function:
** Description:
** *************************************************************************/
void Information::flood_handler(Self * self, Message * message, Notifier * notifier) {
    std::cout << "== [Command] Sending notification for Information request" << '\n';

    // Push message into AwaitContainer
    // unsigned int expected_responses = (unsigned int)this->node_instance->get_connections()->size() + 1;
    std::string await_key = this->node_instance->get_awaiting()->push_request(*message, 1);

    std::string source_id = (*self).id;
    std::string destination_id = message->get_source_id();
    unsigned int nonce = 0;
    std::string network_data = generate_node_info(this->node_instance, this->state);

    Message self_info(source_id, destination_id, INFORMATION_TYPE, RESPOND_PHASE, network_data, nonce);

    this->node_instance->get_awaiting()->push_response(await_key, self_info);

    // source_id += ";" + await_key;
    // destination_id = "ALL";
    // Message notice(source_id, destination_id, QUERY_TYPE, RESPOND_PHASE, "Request for Sensor Readings.", nonce);
    //
    // notifier->send(&notice, NETWORK_NOTICE);
}

/* **************************************************************************
** Function:
** Description:
** *************************************************************************/
void Information::respond_handler() {

}

/* **************************************************************************
** Function:
** Description:
** *************************************************************************/
void Information::close_handler() {

}



/* **************************************************************************
** Function:
** Description:
** *************************************************************************/
Message Query::handle_message(Message * message) {
    Message response;

    this->whatami();
    this->message = message;

    switch ( message->get_phase() ) {
        case FLOOD_PHASE: {
            this->flood_handler(&(*this->state).self, message, this->node_instance->get_notifier());
            break;
        }
        case RESPOND_PHASE: {
            this->respond_handler(&(*this->state).self, message, this->node_instance->get_connection(0));
            break;
        }
        case AGGREGATE_PHASE: {
            this->aggregate_handler(&(*this->state).self, message, this->node_instance->get_message_queue());
            break;
        }
        case CLOSE_PHASE: {
            this->close_handler();
            break;
        }
    }

    return response;
}

/* **************************************************************************
** Function:
** Description:
** *************************************************************************/
void Query::flood_handler(Self * self, Message * message, Notifier * notifier) {
    std::cout << "== [Command] Sending notification for Query request" << '\n';

    // Push message into AwaitContainer
    unsigned int expected_responses = (unsigned int)this->node_instance->get_connections()->size() + 1;
    std::string await_key = this->node_instance->get_awaiting()->push_request(*message, expected_responses);

    std::string source_id = (*self).id;
    std::string destination_id = message->get_source_id();
    unsigned int nonce = 0;
    std::string reading_data = generate_reading();

    Message self_reading(source_id, destination_id, QUERY_TYPE, AGGREGATE_PHASE, reading_data, nonce);

    this->node_instance->get_awaiting()->push_response(await_key, self_reading);

    source_id += ";" + await_key;
    destination_id = "ALL";
    Message notice(source_id, destination_id, QUERY_TYPE, RESPOND_PHASE, "Request for Sensor Readings.", nonce);

    notifier->send(&notice, NETWORK_NOTICE);
}

/* **************************************************************************
** Function:
** Description:
** *************************************************************************/
void Query::respond_handler(Self * self, Message * message, Connection * connection) {
    std::cout << "== [Node] Recieved " << this->message->get_data() << " from " << this->message->get_source_id() << '\n';

    // Send Request to the server
    std::string source_id = (*self).id;
    std::string destination_id = message->get_source_id();
    std::string await_id = message->get_await_id();
    if (await_id != "") {
        destination_id += ";" + await_id;
    }
    unsigned int nonce = message->get_nonce() + 1;
    std::string data = generate_reading();

    Message request(source_id, destination_id, QUERY_TYPE, AGGREGATE_PHASE, data, nonce);

    // TODO: Add method to defer if node_instance is a coordinator
    connection->send(&request);

    // Recieve Response from the server
    Message response = connection->recv();
    std::cout << "== [Node] Recieved: " << response.get_pack() << '\n';
}

/* **************************************************************************
** Function:
** Description:
** *************************************************************************/
void Query::aggregate_handler(Self * self, Message * message, MessageQueue * message_queue) {
    std::cout << "== [Node] Recieved " << message->get_data() << " from " << message->get_source_id() << " thread" << '\n';

    this->node_instance->get_awaiting()->push_response(*message);
    // If ready or if string filled from push response send off to client?

    // Need to track encryption keys and nonces
    std::string source_id = (*self).id;
    std::string destination_id = message->get_source_id();
    unsigned int nonce = message->get_nonce() + 1;
    Message response(source_id , destination_id, QUERY_TYPE, CLOSE_PHASE, "Message Response", nonce);

    message_queue->add_message(message->get_source_id(), response);
    message_queue->push_pipes();

    this->node_instance->notify_connection(message->get_source_id());
}

/* **************************************************************************
** Function:
** Description:
** *************************************************************************/
void Query::close_handler() {

}



/* **************************************************************************
** Function:
** Description:
** *************************************************************************/
Message Election::handle_message(Message * message) {
    Message response;

    this->whatami();
    this->message = message;
    this->phase = this->message->get_phase();

    switch ( ( Phase )this->phase ) {
        case PROBE_PHASE: {
            this->probe_handler();
            break;
        }
        case PRECOMMIT_PHASE: {
            this->precommit_handler();
            break;
        }
        case VOTE_PHASE: {
            this->vote_handler();
            break;
        }
        case ABORT_PHASE: {
            this->abort_handler();
            break;
        }
        case RESULTS_PHASE: {
            this->results_handler();
            break;
        }
        case CLOSE_PHASE: {
            this->close_handler();
            break;
        }
        default: {

            break;
        }
    }

    return response;
}

/* **************************************************************************
** Function:
** Description:
** *************************************************************************/
void Election::probe_handler() {

}

/* **************************************************************************
** Function:
** Description:
** *************************************************************************/
void Election::precommit_handler() {

}

/* **************************************************************************
** Function:
** Description:
** *************************************************************************/
void Election::vote_handler() {

}

/* **************************************************************************
** Function:
** Description:
** *************************************************************************/
void Election::abort_handler() {

}

/* **************************************************************************
** Function:
** Description:
** *************************************************************************/
void Election::results_handler() {

}

/* **************************************************************************
** Function:
** Description:
** *************************************************************************/
void Election::close_handler() {

}



/* **************************************************************************
** Function:
** Description:
** *************************************************************************/
Message Transform::handle_message(Message * message) {
    Message response;

    this->whatami();
    this->message = message;
    this->phase = this->message->get_phase();

    switch ( ( Phase )this->phase ) {
        case INFO_PHASE: {
            this->info_handler();
            break;
        }
        case HOST_PHASE: {
            this->host_handler();
            break;
        }
        case CONNECT_PHASE: {
            this->connect_handler();
            break;
        }
        case CLOSE_PHASE: {
            this->close_handler();
            break;
        }
        default: {

            break;
        }
    }

    return response;
}

/* **************************************************************************
** Function:
** Description:
** *************************************************************************/
void Transform::info_handler() {

}

/* **************************************************************************
** Function:
** Description:
** *************************************************************************/
void Transform::host_handler() {

}

/* **************************************************************************
** Function:
** Description:
** *************************************************************************/
void Transform::connect_handler() {

}

/* **************************************************************************
** Function:
** Description:
** *************************************************************************/
void Transform::close_handler() {

}



/* **************************************************************************
** Function:
** Description:
** *************************************************************************/
Message Connect::handle_message(Message * message) {
    Message response;

    this->whatami();
    this->message = message;
    this->phase = this->message->get_phase();

    switch ( ( Phase )this->phase ) {
        case CONTACT_PHASE: {
            this->contact_handler();
            break;
        }
        case JOIN_PHASE: {
            this->join_handler(&(*this->state).self, &(*this->state).network, message, this->node_instance->get_control());
            break;
        }
        case CLOSE_PHASE: {
            this->close_handler();
            break;
        }
        default: {
            this->node_instance->get_control()->send("\x15");
            break;
        }
    }

    return response;
}

/* **************************************************************************
** Function:
** Description:
** *************************************************************************/
void Connect::contact_handler() {

}

/* **************************************************************************
** Function:
** Description:
** *************************************************************************/
void Connect::join_handler(Self * self, Network * network, Message * message, Control * control) {
    std::cout << "== [Command] Setting up full connection\n";
    std::string full_port = std::to_string(self->next_full_port);

    Connection *full = this->node_instance->setup_wifi_connection(message->get_source_id(), full_port);
    this->node_instance->get_connections()->push_back(full);
    if (full->get_worker_status()) {
        std::cout << "== [Command] Connection worker thread is ready" << '\n';
    }

    std::cout << "== [Command] New connection pushed back\n";
    control->send("\x04");

    (*network).known_nodes++;
}

/* **************************************************************************
** Function:
** Description:
** *************************************************************************/
void Connect::close_handler() {

}
