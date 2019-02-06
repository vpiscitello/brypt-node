#ifndef CONNECTION_HPP
#define CONNECTION_HPP

#include <unistd.h>
#include "utility.hpp"
#include "zmq.hpp"
#include <vector>

class Connection {
    protected:
        bool active;
        bool instantiate_connection;
	
	// each connection should have two named pipes
	bool data_available;
	
	pid_t c_pid;

    public:
        // Method method;
        virtual void whatami() = 0;
        virtual std::string serve() = 0;
        virtual void send(std::string) = 0;
        void unspecial() {
            std::cout << "I am calling an unspecialized function." << '\n';
        }
};

class Direct : public Connection {
    private:
        std::string port;
        std::string peer_IP;
        std::string peer_port;
	bool control;


        zmq::context_t *context;
        zmq::socket_t *socket;
	zmq::pollitem_t item;

    public:
        Direct() {}
        Direct(Options *options) {
	    std::cout << "Creating direct instance.\n";
            this->port = options->port;
            this->peer_IP = options->peer_IP;
            this->peer_port = options->peer_port;
	    this->control = options->is_control;

	    if (options->is_control) {
		std::cout << "[Direct] Creating control socket\n";

		this->context = new zmq::context_t(1);

		switch (options->operation) {
		    case SERVER:
			this->socket = new zmq::socket_t(*this->context, ZMQ_REP);
			this->item.socket = *this->socket;
			this->item.events = ZMQ_POLLIN;
			std::cout << "[Control] Setting up connection on port " << options->port << "... PID: " << getpid() << "\n\n";
			this->instantiate_connection = true;
			this->socket->bind("tcp://*:" + options->port); 
			break;
		    case CLIENT:
			this->socket = new zmq::socket_t(*this->context, ZMQ_REQ);
			std::cout << "Connecting..." << "\n\n";
			this->socket->connect("tcp://" + options->peer_IP + ":" + options->peer_port);
			this->instantiate_connection = false;
			break;
		}
		return;
	    }
	    std::cout << "[Direct] Creating normal socket\n";
	    this->c_pid = fork();
	    switch (this->c_pid) {
		case -1:
		    std::cout << "Error creating child process\n";
		    return;
		case 0:
		    sleep(1);
		    std::cout << "[Direct] The child id is " << getpid() << "\n";
		    this->context = new zmq::context_t(1);

		    switch (options->operation) {
			case SERVER:
			    this->socket = new zmq::socket_t(*this->context, ZMQ_REP);
			    this->item.socket = *this->socket;
			    this->item.events = ZMQ_POLLIN;
			    std::cout << "[Direct] Setting up connection on port " << options->port << "... PID: " << getpid() << "\n\n";
			    this->instantiate_connection = true;
			    this->socket->bind("tcp://*:" + options->port);
			    while (1) {
				std::string req = this->serve();
				if (req != "") {
				    this->send("OK");
				}
			    }
			    break;
			case CLIENT:
			    this->socket = new zmq::socket_t(*this->context, ZMQ_REQ);
			    std::cout << "[Direct] Connecting..." << "\n\n";
			    this->socket->connect("tcp://" + options->peer_IP + ":" + options->peer_port);
			    this->instantiate_connection = false;
			    break;
		    }
		    return;
		default:
		    std::cout << "This parent process id is " << getpid() << " not EXITING\n";
		    //exit(0);
		    return;
	    }
            
        }
        void whatami() {
            std::cout << "I am a Direct implementation." << '\n';
        }
//node class handles getting a whole message, connection class handles the handshake message sending all that
	std::string serve(){
	    do {
		if (zmq_poll(&this->item, 1, 100) >= 0) {
		    if (this->item.revents == 0) {
			return "";
		    }
		    std::cout << "[Direct] Receiving... PID: " << getpid() << '\n';
		    zmq::message_t request;
		    this->socket->recv(&request);
		    std::string req = std::string(static_cast<char *>(request.data()), request.size());
		    std::cout << "[Direct] Received: " << req << "\n";
		    return req;
		} else {
		    std::cout << "Code: " << zmq_errno() << " message: " << zmq_strerror(zmq_errno()) << "\n";
		    exit(1);
		}
	    } while ( true );
	}

	void send(std::string message) {
	    std::cout << "[Direct] Sending..." << '\n';
	    zmq::message_t request( message.size() );
	    memcpy( request.data(), message.c_str(), message.size() );
	    this->socket->send( request );
	    std::cout << "[Direct] Sent: " << message << '\n';
        }
};

class Bluetooth : public Connection {
    public:
        void whatami() {
            std::cout << "I am a BLE implementation." << '\n';
        }
	
	std::string serve(){

	}
	
	void send(std::string message) {

	}
};

class LoRa : public Connection {
    public:
        void whatami() {
            std::cout << "I am a LoRa implementation." << '\n';
        }

	std::string serve(){

	}
	
	void send(std::string message) {

	}
};

class Websocket : public Connection {
    public:
        void whatami() {
            std::cout << "I am a Websocket implementation." << '\n';
        }

	std::string serve(){

	}
	
	void send(std::string message) {

	}
};

inline Connection* ConnectionFactory(TechnologyType technology) {
    switch (technology) {
        case DIRECT_TYPE:
            return new Direct;
            break;
        case BLE_TYPE:
            return new Bluetooth;
            break;
        case LORA_TYPE:
            return new LoRa;
            break;
        case WEBSOCKET_TYPE:
            return new Websocket;
            break;
    }
    return NULL;
}

inline Connection* ConnectionFactory(TechnologyType technology, Options *options) {
    switch (technology) {
        case DIRECT_TYPE:
            return new Direct(options);
            break;
        case BLE_TYPE:
            return new Bluetooth();
            break;
        case LORA_TYPE:
            return new LoRa();
            break;
        case WEBSOCKET_TYPE:
            return new Websocket();
            break;
    }
    return NULL;
}

#endif
