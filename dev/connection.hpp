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

        void serve() {
            do {
                std::cout << "Receiving..." << '\n';
		zmq::message_t request;
		this->socket->recv( &request );
                std::cout << std::string( static_cast< char * >( request.data() ), request.size() ) << "\n\n";

                sleep( 2 );

		std::string message = "Response.";
		zmq::message_t reply( message.size() );
		memcpy( reply.data(), message.c_str(), message.size() );
		this->socket->send( reply );
	    } while ( true );
        }

        void send() {
            do {
                std::cout << "Sending..." << '\n';
                std::string message = "Request.";
		zmq::message_t request( message.size() );
		memcpy( request.data(), message.c_str(), message.size() );
		this->socket->send( request );

		zmq::message_t reply;
		this->socket->recv( &reply );
		std::cout << std::string( static_cast< char * >( reply.data() ), reply.size() ) << "\n\n";

		sleep( 5 );
	    } while ( true );
        }

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
                    std::cout << "Serving..." << "\n\n";
                    this->instantiate_connection = true;
                    this->socket->bind("tcp://*:" + options->port);
                    this->serve();
                    break;
                case CLIENT:
                    this->socket = new zmq::socket_t(*this->context, ZMQ_REQ);
                    std::cout << "Connecting..." << "\n\n";
                    this->socket->connect("tcp://" + options->peer_IP + ":" + options->peer_port);
                    this->instantiate_connection = false;
                    this->send();
                    break;
            }
        }
        void whatami() {
            std::cout << "I am a Direct implementation." << '\n';
        }
};

class Control : public Connection {
    private:
        std::string port;
        std::string peer_IP;
        std::string peer_port;

	//std::vector<pthread_t *> threads;

        zmq::context_t *context;
        zmq::socket_t *socket;
	zmq::pollitem_t *item;

	static void * connection_handler(void * args) {
	    std::cout << "inside connection handler\n";
	    int port_int = (int)(uintptr_t) args;
	    char * outstr = new char[200];
	    sprintf(outstr, "%d", port_int);
	    std::string port = outstr;

	    zmq::context_t *context2;
	    zmq::socket_t *socket2;

	    context2 = new zmq::context_t(1);
	    socket2 = new zmq::socket_t(*context2, ZMQ_REP);

	    std::cout << "Serving..." << "\n";
	    socket2->bind("tcp://*:" + port);
	    std::cout << "On port: " << port << "\n";
	    zmq::message_t request;
	    socket2->recv( &request );
	    std::string req = std::string(static_cast<char *>(request.data()), request.size());
	    std::cout << "RECEIEVED: " <<  req << "\n";

	    return NULL;
	}

	void serve(){
	    do {
		if (zmq_poll(this->item, 1, 100) >= 0) {
		    if (this->item->revents == 0) {
			continue;
		    }
		    std::cout << "ITEM AVAILABLE: " << this->item->revents << "\n";
		    std::cout << "Receiving..." << '\n';
		    zmq::message_t request;
		    this->socket->recv( &request );
		    std::string req = std::string(static_cast<char *>(request.data()), request.size());
		    std::cout << req << "\n";
		    if (req == "CONNECT") {
			std::cout << "IT IS CONNECT\n";
			//get a new port and set it up
			int port = 3010;
			char * outstr = new char[200];
			sprintf(outstr, "%d", port);
			std::string message = outstr;
			std::cout << "Message is " << message << "\n";
			zmq::message_t reply(message.size());
			memcpy(reply.data(), message.c_str(), message.size());
			//create a new thread to listen
			pthread_t new_thread;
			if(pthread_create(&new_thread, NULL, connection_handler, (void *)(uintptr_t)port)) {
			    fprintf(stderr, "error creating connection thread\n");
			    return;
			}
			std::cout << "Sending: " << message << "\n";
			this->socket->send(reply);

			if (pthread_join(new_thread, NULL)) {
			    fprintf(stderr, "error joining thread\n");
			    return;
			}
		    }
		    sleep( 2 );
		} else {
		    std::cout << "Code: " << zmq_errno() << " message: " << zmq_strerror(zmq_errno()) << "\n";
		}
	    } while ( true );
	}

	void send(){
	    do {
		std::cout << "Sending..." << '\n';
		std::string message = "Request.";
		zmq::message_t request( message.size() );
		memcpy( request.data(), message.c_str(), message.size() );
		this->socket->send( request );

		zmq::message_t reply;
		this->socket->recv( &reply );
		std::cout << std::string( static_cast< char * >( reply.data() ), reply.size() ) << "\n\n";

		sleep( 5 );
	    } while ( true );
	}

    public:
        Control() {}
        Control(Options *options) {
            this->port = options->port;
            this->peer_IP = options->peer_IP;
            this->peer_port = options->peer_port;

            this->context = new zmq::context_t(1);

            switch (options->operation) {
                case SERVER:
                    this->socket = new zmq::socket_t(*this->context, ZMQ_REP);
					*this->item = zmq::pollitem_t{*this->socket, 0, ZMQ_POLLIN, 0};
                    std::cout << "Control SERVER Serving..." << "\n\n";
                    this->instantiate_connection = true;
                    this->socket->bind("tcp://*:" + options->port);
					std::cout << "On port: " << options-> port << "\n";
                    this->serve();
                    break;
                case CLIENT:
                    this->socket = new zmq::socket_t(*this->context, ZMQ_REQ);
                    std::cout << "Connecting..." << "\n\n";
                    this->socket->connect("tcp://" + options->peer_IP + ":" + options->peer_port);
                    this->instantiate_connection = false;
                    this->send();
                    break;
            }
        }
        void whatami() {
            std::cout << "I am a Control implementation." << '\n';
        }
};


class Bluetooth : public Connection {
    public:
        void whatami() {
            std::cout << "I am a BLE implementation." << '\n';
        }
};

class LoRa : public Connection {
    public:
        void whatami() {
            std::cout << "I am a LoRa implementation." << '\n';
        }
};

class Websocket : public Connection {
    public:
        void whatami() {
            std::cout << "I am a Websocket implementation." << '\n';
        }
};

inline Connection* ConnectionFactory(TechnologyType technology) {
    switch (technology) {
        case DIRECT_TYPE:
            return new Direct;
            break;
        case CONTROL_TYPE:
            return new Control;
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
}

inline Connection* ConnectionFactory(TechnologyType technology, Options *options) {
    switch (technology) {
        case DIRECT_TYPE:
            return new Direct(options);
            break;
        case CONTROL_TYPE:
            return new Control(options);
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
}

#endif
