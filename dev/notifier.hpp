#ifndef NOTIFIER_HPP
#define NOTIFIER_HPP

#include <string>

#include "zmq.hpp"

#include "utility.hpp"

class Notifier {
    private:
        struct State * node_state;

        zmq::context_t context;
        zmq::socket_t publisher;
        zmq::socket_t subscriber;

        std::string network_prefix = "network.all:";
        std::string cluster_prefix = "cluster.";
        std::string node_prefix = "node.";

        bool subscribed = false;

    public:
    	Notifier(struct State * state, std::string port) : context(1), publisher(context, ZMQ_PUB), subscriber(context, ZMQ_SUB) {
            this->node_state = state;
            std::cout << "== [Notifier] Setting up publisher socket on port " + port + "\n";
            publisher.bind("tcp://*:" + port);
    	}

    	void restart() {

    	}

        std::string get_prefix(NotificationType type) {
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

        void connect(std::string ip, std::string port) {
            std::cout << "== [Notifier] Subscribing to peer at " + ip + ":" + port + "\n";
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
    	void send(Message * message, NotificationType type) {
            std::string raw_message = message->get_pack();
            std::string notification = this->get_prefix(type) + raw_message;

            zmq::message_t zmq_notification(notification.size());
            memcpy(zmq_notification.data(), notification.c_str(), notification.size());

    	    this->publisher.send(zmq_notification);
    	}

        // Passthrough for send function of the connection type
        // Update to target sub clusters and nodes in the prefix
        void send(std::string message, NotificationType type) {
            std::string notification = this->get_prefix(type) + message;

            zmq::message_t zmq_notification(notification.size());
            memcpy(zmq_notification.data(), notification.c_str(), notification.size());

            this->publisher.send(zmq_notification);
        }

        // Receive for requests, if a request is received recv it and then return the message string
        std::string recv() {
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
            std::cout << "== [Notifier] Recieved: " << notification << "\n";

            return notification;
        }

};

#endif
