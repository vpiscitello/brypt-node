#ifndef CONNECTION_HPP
#define CONNECTION_HPP

#include <unistd.h>
#include "utility.hpp"
#include "zmq.hpp"
#include <pthread.h>
#include <vector>

class Connection {
    protected:
        bool active;
        bool instantiate_connection;
    public:
        // Method method;
        virtual void whatami() = 0;
        virtual void serve(int) = 0;
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

        zmq::context_t *context;
        zmq::socket_t *socket;
	zmq::pollitem_t item;

    public:
        Direct() {}
        Direct(Options *options) {
            this->port = options->port;
            this->peer_IP = options->peer_IP;
            this->peer_port = options->peer_port;

            this->context = new zmq::context_t(1);

            switch (options->operation) {
                case SERVER:
                    this->socket = new zmq::socket_t(*this->context, ZMQ_REP);
		    this->item.socket = *this->socket;
		    this->item.events = ZMQ_POLLIN;
                    std::cout << "Serving..." << "\n\n";
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
        }
        void whatami() {
            std::cout << "I am a Direct implementation." << '\n';
        }

	void serve(int message_size){
	    do {
		if (zmq_poll(&this->item, 1, 100) >= 0) {
		    if (this->item.revents == 0) {
			continue;
		    }
		    std::cout << "Receiving..." << '\n';
		    zmq::message_t request;
		    this->socket->recv( &request );
		    std::string req = std::string(static_cast<char *>(request.data()), request.size());
		    std::cout << "Received: " << req << "\n";
		    
		    sleep( 2 );

		    std::string message = "Response.";
		    this->send(message);
		} else {
		    std::cout << "Code: " << zmq_errno() << " message: " << zmq_strerror(zmq_errno()) << "\n";
		}
	    } while ( true );
	}

	void send(std::string message) {
	    std::cout << "Sending..." << '\n';
	    zmq::message_t request( message.size() );
	    memcpy( request.data(), message.c_str(), message.size() );
	    this->socket->send( request );
	    std::cout << "Sent: " << message << '\n';
        }
};

//class Control : public Connection {
//    private:
//        std::string port;
//
//	//std::vector<pthread_t *> threads;
//
//        zmq::context_t *context;
//        zmq::socket_t *socket;
//	zmq::pollitem_t item;
//
//	std::string get_free_port() {
//	    int port = 3010;
//	    char * outstr = new char[10];
//	    sprintf(outstr, "%d", port);
//	    std::string message = outstr;
//	    return message;
//	}
//
//	void handle_req_type(std::string req) {
//	    if (req == "CONNECT") {
//		std::cout << "Received request for connection\n";
//		//get a new port and set it up
//		std::string port = get_free_port();
//		std::cout << "Port is " << port << "\n";
//		//zmq::message_t reply(port.size());
//		//memcpy(reply.data(), port.c_str(), port.size());
//		//create a new thread to listen
//		pthread_t new_thread;
//		if(pthread_create(&new_thread, NULL, connection_handler, (void *)(uintptr_t)std::stoi(port))) {
//		    fprintf(stderr, "error creating connection thread\n");
//		    return;
//		}
//		std::cout << "Sending: " << port << "\n";
//		this->send(port);
//
//		if (pthread_join(new_thread, NULL)) {
//		    fprintf(stderr, "error joining thread\n");
//		    return;
//		}
//	    }
//	}
//
//	static void * connection_handler(void * args) {
//	    std::cout << "Control socket thread\n";
//	    int port_int = (int)(uintptr_t) args;
//	    char * outstr = new char[200];
//	    sprintf(outstr, "%d", port_int);
//	    std::string port = outstr;
//
//	    zmq::context_t *context2;
//	    zmq::socket_t *socket2;
//
//	    context2 = new zmq::context_t(1);
//	    socket2 = new zmq::socket_t(*context2, ZMQ_REP);
//
//	    std::cout << "Control socket thread serving..." << "\n";
//	    socket2->bind("tcp://*:" + port);
//	    std::cout << "On port: " << port << "\n";
//	    zmq::message_t request;
//	    std::cout << "Control socket thread receiving..." << '\n';
//	    socket2->recv( &request );
//	    std::string req = std::string(static_cast<char *>(request.data()), request.size());
//	    std::cout << "Control socket thread received: " <<  req << "\n";
//
//	    return NULL;
//	}
//
//	
//
//    public:
//        Control() {}
//        Control(Options *options) {
//            this->port = options->port;
//
//            this->context = new zmq::context_t(1);
//
//            switch (options->operation) {
//                case SERVER:
//                    this->socket = new zmq::socket_t(*this->context, ZMQ_REP);
//		    this->item.socket = *this->socket;
//		    this->item.events = ZMQ_POLLIN;
//                    std::cout << "Control socket serving..." << "\n";
//                    this->instantiate_connection = true;
//                    this->socket->bind("tcp://*:" + options->port);
//		    std::cout << "On port: " << options->port << "\n";
//                    break;
//                case CLIENT:
//                    this->socket = new zmq::socket_t(*this->context, ZMQ_REQ);
//                    std::cout << "Connecting..." << "\n";
//                    //this->socket->connect("tcp://" + options->peer_IP + ":" + options->peer_port);
//                    this->instantiate_connection = false;
//                    break;
//            }
//        }
//        void whatami() {
//            std::cout << "I am a Control implementation." << '\n';
//        }
//
	//void serve(int message_size){
	//    do {
	//	if (zmq_poll(&this->item, 1, 100) >= 0) {
	//	    if (this->item.revents == 0) {
	//		continue;
	//	    }
	//	    std::cout << "Control socket receiving..." << '\n';
	//	    zmq::message_t request;
	//	    this->socket->recv( &request );
	//	    std::string req = std::string(static_cast<char *>(request.data()), request.size());
	//	    std::cout << "Control socket received: " << req << "\n";
	//	    handle_req_type(req);
	//	    
	//	    sleep( 2 );
	//	} else {
	//	    std::cout << "Code: " << zmq_errno() << " message: " << zmq_strerror(zmq_errno()) << "\n";
	//	}
	//    } while ( true );
	//}
//
//	void send(std::string message) {
//	    std::cout << "Sending..." << '\n';
//	    zmq::message_t request( message.size() );
//	    memcpy( request.data(), message.c_str(), message.size() );
//	    this->socket->send( request );
//	}
//};


class Bluetooth : public Connection {
    public:
        void whatami() {
            std::cout << "I am a BLE implementation." << '\n';
        }
	
	void serve(int message_size){

	}
	
	void send(std::string message) {

	}
};

class LoRa : public Connection {
    public:
        void whatami() {
            std::cout << "I am a LoRa implementation." << '\n';
        }

	void serve(int message_size){

	}
	
	void send(std::string message) {

	}
};

class Websocket : public Connection {
    public:
        void whatami() {
            std::cout << "I am a Websocket implementation." << '\n';
        }

	void serve(int message_size){

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
