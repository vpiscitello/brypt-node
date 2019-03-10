#ifndef NOTIFIER_HPP
#define NOTIFIER_HPP

#include <string>

#include "zmq.hpp"

#include "utility.hpp"

class Notifier {
    private:
        zmq::context_t context;
        zmq::socket_t publisher;
        zmq::socket_t subscriber;

        bool subscribed = false;

    public:
    	Notifier(std::string port) : context(1), publisher(context, ZMQ_PUB), subscriber(context, ZMQ_SUB) {
            std::cout << "== [Node] Setting up publisher socket on port " + port + "\n";
            publisher.bind("tcp://*:" + port);
    	}

    	void restart() {

    	};

        void connect(std::string ip, std::string port) {
            std::cout << "== [Node] Subscribing to peer at " + ip + ":" + port + "\n";
            subscriber.connect("tcp://" + ip + ":" + port);
            this->subscribed = true;
        }

    	// Passthrough for send function of the connection type
    	void send(Message * message) {
            std::string raw_message = message->get_pack();

            zmq::message_t notification(raw_message.size());
            memcpy(notification.data(), raw_message.c_str(), raw_message.size());

    	    this->publisher.send(notification);
    	}

        // Passthrough for send function of the connection type
        void send(std::string message) {
            zmq::message_t notification(message.size());
            memcpy(notification.data(), message.c_str(), message.size());

            this->publisher.send(notification);
        }

        // Receive for requests, if a request is received recv it and then return the message string
        std::string recv() {
            if (!this->subscribed) {
                return "";
            }
            zmq::message_t notification;

            this->subscriber.recv(&notification, ZMQ_NOBLOCK);

            return "";
        };

};

#endif
