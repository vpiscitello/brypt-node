#include "notifier.hpp"

Notifier::Notifier(struct State * state, class Node * node_instance, std::string port) : context(1), publisher(context, ZMQ_PUB), subscriber(context, ZMQ_SUB) {
    this->node_state = state;
    this->node_instance = node_instance;

    printo("Setting up publisher socket on port " + port, CONTROL_P);
    publisher.bind("tcp://*:" + port);
}

void Notifier::restart() {

}

std::string Notifier::get_prefix(NotificationType type) {
    switch (type) {
	case NETWORK_NOTICE: {
	    return this->network_prefix;
	}
	case CLUSTER_NOTICE: {
	    return this->cluster_prefix;
	}
	case NODE_NOTICE: {
	    return this->node_prefix;
	}
	default: {
	    return "";
	}
    }
}

void Notifier::connect(std::string ip, std::string port) {
    printo("Subscribing to peer at " + ip + ":" + port, CONTROL_P);
    this->subscriber.connect("tcp://" + ip + ":" + port);

    this->cluster_prefix = "cluster." + (*this->node_state).coordinator.id + ":";
    this->node_prefix = "node." + (*this->node_state).self.id + ":";

    this->subscriber.setsockopt(ZMQ_SUBSCRIBE, network_prefix.c_str(), network_prefix.length());
    this->subscriber.setsockopt(ZMQ_SUBSCRIBE, cluster_prefix.c_str(), cluster_prefix.length());
    this->subscriber.setsockopt(ZMQ_SUBSCRIBE, node_prefix.c_str(), node_prefix.length());

    this->subscribed = true;
}

// Passthrough for send function of the connection type
// Update to target sub clusters and nodes in the prefix
void Notifier::send(Message * message, NotificationType type) {
    std::string raw_message = message->get_pack();
    std::string notification = this->get_prefix(type) + raw_message;

    zmq::message_t zmq_notification(notification.size());
    memcpy(zmq_notification.data(), notification.c_str(), notification.size());

    // Check the type and send via a standard message if it is not something that supports a pub/sub socket
    for (int idx = 0; idx < (int)(this->node_instance->get_connections()->size()); idx++) {
	std::string internal_conn_type = this->node_instance->get_connection(idx)->get_internal_type();
	if (internal_conn_type == "StreamBridge" || internal_conn_type == "TCP" || internal_conn_type == "BLE" || internal_conn_type == "LoRa") {
	    // Instead send just a normal message
	    this->node_instance->get_connection(idx)->send(message->get_pack().c_str());
	}
    }

    this->publisher.send(zmq_notification);
}

// Passthrough for send function of the connection type
// Update to target sub clusters and nodes in the prefix
void Notifier::send(std::string message, NotificationType type) {
    std::string notification = this->get_prefix(type) + message;

    zmq::message_t zmq_notification(notification.size());
    memcpy(zmq_notification.data(), notification.c_str(), notification.size());

    // Check the type and send via a standard message if it is not something that supports a pub/sub socket
    for (int idx = 0; idx < (int)(this->node_instance->get_connections()->size()); idx++) {
    	std::string internal_conn_type = this->node_instance->get_connection(idx)->get_internal_type();
    	if (internal_conn_type == "StreamBridge" || internal_conn_type == "TCP" || internal_conn_type == "BLE" || internal_conn_type == "LoRa") {
    	    // Instead send just a normal message
    	    printo("Contacting unsubcribed connections", CONTROL_P);
    	    this->node_instance->get_connection(idx)->send(message.c_str());
    	}
    }

    this->publisher.send(zmq_notification);
}

// Receive for requests, if a request is received recv it and then return the message string
std::string Notifier::recv() {
    if (!this->subscribed) {
	return "";
    }

    int recvd_bytes = 0;
    std::string notification = "";

    zmq::message_t message;
    recvd_bytes = this->subscriber.recv(&message, ZMQ_DONTWAIT);
    if (recvd_bytes < 1) {
	return notification;
    }

    notification = std::string(static_cast<char *>(message.data()), message.size());
    printo("Recieved: " + notification, CONTROL_P);

    return notification;
}
