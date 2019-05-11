#ifndef NOTIFIER_HPP
#define NOTIFIER_HPP

#include <string>

#include "zmq.hpp"

#include "utility.hpp"
#include "node.hpp"

class Notifier {
    private:
        struct State * node_state;
	class Node * node_instance;

        zmq::context_t context;
        zmq::socket_t publisher;
        zmq::socket_t subscriber;

        std::string network_prefix = "network.all:";
        std::string cluster_prefix = "cluster.";
        std::string node_prefix = "node.";

        bool subscribed = false;

    public:
    	Notifier(struct State * state, class Node * node_instance, std::string port);
    	void restart();
        std::string get_prefix(NotificationType type);
        void connect(std::string ip, std::string port);
    	// Passthrough for send function of the connection type
        // Update to target sub clusters and nodes in the prefix
    	void send(Message * message, NotificationType type);
        // Passthrough for send function of the connection type
        // Update to target sub clusters and nodes in the prefix
        void send(std::string message, NotificationType type);
        // Receive for requests, if a request is received recv it and then return the message string
        std::string recv();

};

#endif
