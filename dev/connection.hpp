#ifndef CONNECTION_HPP
#define CONNECTION_HPP

#include <unistd.h>


#include "zmq.hpp"

#include "utility.hpp"
#include "message.hpp"

// Includes to be moved to commands
#include "json11.hpp" // JSON handling
#include <time.h> // Random reading
#include <chrono> // Timestamp


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

        void serve() {
            srand(time(NULL));
            do {
                std::cout << "Receiving..." << '\n';
                zmq::message_t request;
                this->socket->recv( &request );

                std::string req_raw( static_cast< char * >( request.data() ), request.size() );
                class Message req_message( req_raw );
                std::cout << req_raw << "\n" << req_message.get_data() << "\n\n";

                std::string node_id = "1";
                CommandType command = req_message.get_command();
                int phase = req_message.get_phase() + 1;
                std::string data;
                unsigned int nonce = req_message.get_nonce() + 1;

                switch(req_message.get_command()) {

                    case INFORMATION_TYPE: {
                        // Move to utility header (will be used in message class and command class
                        std::stringstream epoch_ss;
                        std::chrono::seconds seconds;
                        std::chrono::time_point<std::chrono::system_clock> current_time;
                        current_time = std::chrono::system_clock::now();
                        seconds = std::chrono::duration_cast<std::chrono::seconds>( current_time.time_since_epoch() );
                        epoch_ss.clear();
                        epoch_ss << seconds.count();
                        std::string epoch_str = epoch_ss.str();


                        json11::Json nodes_json = json11::Json::array {
                            json11::Json::object {
                                { "uid", "1" },
                                { "cluster", 0 },
                                { "coordinator", 1 },
                                { "neighbor_count", 4 },
                                { "designation", "root" },
                                { "comm_techs", json11::Json::array { "WiFi" } },
                                { "update_timestamp", epoch_str }
                            },
                            json11::Json::object {
                                { "uid", "2" },
                                { "cluster", 0 },
                                { "coordinator", 1 },
                                { "neighbor_count", 4 },
                                { "designation", "node" },
                                { "comm_techs", json11::Json::array { "WiFi" } },
                                { "update_timestamp", epoch_str }
                            },
                            json11::Json::object {
                                { "uid", "3" },
                                { "cluster", 0 },
                                { "coordinator", 1 },
                                { "neighbor_count", 6 },
                                { "designation", "coordinator" },
                                { "comm_techs", json11::Json::array { "WiFi", "LoRa" } },
                                { "update_timestamp", epoch_str }
                            },
                            json11::Json::object {
                                { "uid", "6" },
                                { "cluster", 0 },
                                { "coordinator", 0 },
                                { "neighbor_count", 4 },
                                { "designation", "node" },
                                { "comm_techs", json11::Json::array { "WiFi" } },
                                { "update_timestamp", epoch_str }
                            }
                        };

                        data = nodes_json.dump();

                        break;
                    }

                    case QUERY_TYPE: {

                        json11::Json::object msg_obj;

                        for(int idx = 1; idx <= 6; idx++) {
                            int reading = rand() % ( 74 - 68 ) + 68;

                            // Move to utility header (will be used in message class and command class
                            std::stringstream epoch_ss;
                            std::chrono::seconds seconds;
                            std::chrono::time_point<std::chrono::system_clock> current_time;
                            current_time = std::chrono::system_clock::now();
                            seconds = std::chrono::duration_cast<std::chrono::seconds>( current_time.time_since_epoch() );
                            epoch_ss.clear();
                            epoch_ss << seconds.count();
                            std::string epoch_str = epoch_ss.str();

                            json11::Json reading_json = json11::Json::object({
                                { "reading", reading },
                                { "timestamp", epoch_str }
                            });
                            std::string r_data = reading_json.dump();

                            class Message msg( std::to_string(idx), command, phase, r_data, nonce );
                            std::string msg_pack = msg.get_pack();

                            if(idx == 6) {
                                int include_con = rand() % 100;
                                if(include_con < 23) {
                                    msg_obj[std::to_string(idx)] = "";
                                    break;
                                }
                            }

                            msg_obj[std::to_string(idx)] = msg_pack;

                        }


                        json11::Json msg_json = json11::Json::object(msg_obj);
                        data.append(msg_json.dump());

                        break;
                    }
                    default: {
                        break;
                    }
                }

                // Send message up to Node to handle the command Request
                // Wait for command response from Node
                // Send command response

                class Message rep_message( node_id, command, phase, data, nonce );

                std::string response = rep_message.get_pack();

                zmq::message_t reply( response.size() );
                memcpy( reply.data(), response.c_str(), response.size() );
                this->socket->send( reply );
            } while ( true );
        }

        void send(){
            do {
		    std::cout << "Sending..." << '\n';

		    std::string node_id = "1";
    	    CommandType command = QUERY_TYPE;
		    int phase = 0;
		    std::string data = "Request.";
		    unsigned int nonce = 0;

		    class Message req_message( node_id, command, phase, data, nonce );
		    std::string req = req_message.get_pack();

		    zmq::message_t request( req.size() );
		    memcpy( request.data(), req.c_str(), req.size() );
		    this->socket->send( request );

		    // Send message up to Node to handle the command Request
		    // Wait for command response from Node
		    // Send command response
		    zmq::message_t reply;
		    this->socket->recv( &reply );

		    //std::cout << std::string( static_cast< char * >( reply.data() ), reply.size() ) << "\n\n";
		    std::string rep_raw( static_cast< char * >( reply.data() ), reply.size() );
		    class Message rep_message( rep_raw );
		    std::cout << rep_raw << "\n" << rep_message.get_data() << "\n\n";
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
