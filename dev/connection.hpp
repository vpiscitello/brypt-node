#ifndef CONNECTION_HPP
#define CONNECTION_HPP

#include <unistd.h>
#include "zmq.hpp"

#include "utility.hpp"
#include "message.hpp"

class Connection {
    protected:
        bool active;
        bool instantiate_connection;
    public:
        // Method method;
        virtual void whatami() = 0;
        void pass_up_message() {

        }
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

        void serve(){
            do {
                std::cout << "Receiving..." << '\n';
        		zmq::message_t request;
        		this->socket->recv( &request );
                std::cout << std::string( static_cast< char * >( request.data() ), request.size() ) << "\n\n";

                sleep( 2 );

                // Send message up to Node to handle the command Request
                // Wait for command response from Node
                // Send command response

        		std::string message = "Response.";
        		zmq::message_t reply( message.size() );
        		memcpy( reply.data(), message.c_str(), message.size() );
        		this->socket->send( reply );
        	} while ( true );
        }

        void send(){
            do {
                std::cout << "Sending..." << '\n';
                std::string message = "Request.";
    			zmq::message_t request( message.size() );
    			memcpy( request.data(), message.c_str(), message.size() );
    			this->socket->send( request );

                // Send message up to Node to handle the command Request
                // Wait for command response from Node
                // Send command response

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
                    // this->socket->bind("tcp://" + options->IP + ":" + options->port);
                    this->serve();
                    break;
                case CLIENT:
                    this->socket = new zmq::socket_t(*this->context, ZMQ_REQ);
                    std::cout << "Connecting..." << "\n\n";
                    std::cout << "tcp://" + options->peer_IP + ":" + options->peer_port << '\n';
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
