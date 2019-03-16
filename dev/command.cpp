#include "command.hpp"

/* **************************************************************************
** Function:
** Description:
** *************************************************************************/
Message Information::handle_message(Message * message) {
    Message response;

    this->whatami();
    this->message = message;
    switch ( ( Phase )this->phase ) {
        case PRIVATE_PHASE: {
            this->private_handler();
            break;
        }
        case NETWORK_PHASE: {
            this->network_handler();
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
void Information::private_handler() {

}

/* **************************************************************************
** Function:
** Description:
** *************************************************************************/
void Information::network_handler() {

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
            this->respond_handler(&(*this->state).self, &(*this->state).coordinator, message, this->node_instance->get_connection(0));
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
    std::cout << "== [Node] Sending notification for Query request" << '\n';

    // Push message into AwaitContainer
    // Need to figure out how to attach this.id and hash of message as the destination
    unsigned int expected_responses = (unsigned int)this->node_instance->get_connections()->size();
    std::string await_key = this->node_instance->get_awaiting()->push_request(*message, expected_responses);

    std::string source_id = (*self).id + ";" + await_key;
    std::string destination_id = "ALL";
    unsigned int nonce = 0;
    Message notice(source_id, destination_id, QUERY_TYPE, RESPOND_PHASE, "Request for Sensor Readings.", nonce);

    notifier->send(&notice);
}

/* **************************************************************************
** Function:
** Description:
** *************************************************************************/
void Query::respond_handler(Self * self, Coordinator * coordinator, Message * message, Connection * connection) {
    std::cout << "== [Node] Recieved " << this->message->get_data() << " from " << this->message->get_source_id() << '\n';

    // Send Request to the server
    std::string source_id = (*self).id;
    std::string destination_id = message->get_source_id();
    std::string await_id = message->get_await_id();
    if (await_id != "") {
        destination_id += ";" + await_id;
    }
    unsigned int nonce = message->get_nonce() + 1;
    Message request(source_id, destination_id, QUERY_TYPE, AGGREGATE_PHASE, "Here is some work!", nonce);

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
