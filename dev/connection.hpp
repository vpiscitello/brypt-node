#ifndef CONNECTION_HPP
#define CONNECTION_HPP

#include <unistd.h>
#include <vector>
#include <fstream>

#include "zmq.hpp"

#include "utility.hpp"
#include "message.hpp"

class Connection {
    protected:
        bool active;
        bool instantiate_connection;

        std::string peer_name;

        std::fstream pipe;
        std::string pipe_name;
        unsigned long message_sequence;
    	pid_t c_pid;

    public:
        virtual void whatami() = 0;
        virtual std::string recv() = 0;
        virtual void send(Message *) = 0;
        virtual void send(const char * message) = 0;
        virtual void shutdown() = 0;

        bool get_status() {
            return this->active;
        }

        std::string get_pipe_name() {
            return this->pipe_name;
        }

        bool create_pipe() {
            if (this->peer_name == "") {
                return false;
            }

            std::string filename = this->peer_name + ".pipe";

            this->pipe_name = filename;
            this->pipe.open(filename, std::fstream::in | std::fstream::out);

            if( this->pipe.fail() ){
                this->pipe.close();
                return false;
            }

            return true;
        }

        bool write_to_pipe(std::string message) {
            if ( !this->pipe.is_open() ) {
                return false;
            }

            this->pipe.write( message.c_str(), message.size() );
            return true;
        }

        std::string read_from_pipe() {
            if ( !this->pipe.is_open() ) {
                return "";
            }

            std::string raw_message;
            std::getline(this->pipe, raw_message);

            return raw_message;
        }

        void unspecial() {
            std::cout << "I am calling an unspecialized function." << '\n';
        }

};

class Direct : public Connection {
    private:
        bool control;

        std::string port;
        std::string peer_IP;
        std::string peer_port;

        zmq::context_t *context;
        zmq::socket_t *socket;
        zmq::pollitem_t item;

    public:
        Direct() {}
        Direct(Options *options) {

            std::cout << "== [Control] Creating direct instance.\n";

            this->port = options->port;
            this->peer_IP = options->peer_IP;
            this->peer_port = options->peer_port;
            this->control = options->is_control;

    	    if (options->is_control) {
        		std::cout << "== [Control] Creating control socket\n";

        		this->context = new zmq::context_t(1);

        		switch (options->operation) {
                    case SERVER: {
                        std::cout << "== [Control] Setting up REP socket on port " << options->port << "\n\n";
                        this->setup_rep_socket(options->port);
                        break;
                    }
                    case CLIENT: {
                        std::cout << "== [Control] Connecting REQ socket to " << options->peer_IP << ":" << options->peer_port << "\n\n";
                        this->setup_req_socket(options->peer_IP, options->peer_port);
                        break;
                    }
        		}

        		return;

    	    }

    	    std::cout << "== [Control] Creating DIRECT connection thread\n";
    	    this->c_pid = fork();

    	    switch (this->c_pid) {
        		case -1: {
                    std::cout << "Error creating child process\n";
                    return;
                }

        		case 0: {
                    sleep(1);
                    std::cout << "== [Direct] Connection thread PID " << getpid() << "\n";
                    this->context = new zmq::context_t(1);

                    switch (options->operation) {
                        case SERVER: {
                            std::cout << "== [Direct] Setting up REP socket on port " << options->port << "\n\n";
                            this->setup_rep_socket(options->port);

                            handle_messaging();
                            break;
                        }
                        case CLIENT: {
                            std::cout << "== [Direct] Connecting REQ socket to " << options->peer_IP << ":" << options->peer_port << "\n\n";
                            this->setup_req_socket(options->peer_IP, options->peer_port);

                            break;
                        }
                    }
                    return;
                }

        		default: {
                    return;
                }
    	    }

        }

        void whatami() {
            std::cout << "I am a Direct implementation." << '\n';
        }

        void setup_rep_socket(std::string port) {
            this->socket = new zmq::socket_t(*this->context, ZMQ_REP);
            this->item.socket = *this->socket;
            this->item.events = ZMQ_POLLIN;
            this->instantiate_connection = true;
            this->socket->bind("tcp://*:" + port);
        }

        void setup_req_socket(std::string ip, std::string port) {
            this->socket = new zmq::socket_t(*this->context, ZMQ_REQ);
            this->item.socket = *this->socket;
            this->item.events = ZMQ_POLLIN;
            this->socket->connect("tcp://" + ip + ":" + port);
            this->instantiate_connection = false;
        }

    	void send(Message * message) {
    	    std::cout << "== [Direct] Sending..." << '\n';

    	    std::string msg_pack = message->get_pack();
    	    zmq::message_t request(msg_pack.size());
    	    memcpy(request.data(), msg_pack.c_str(), msg_pack.size());

    	    this->socket->send(request);

    	    std::cout << "== [Direct] Sent: " << msg_pack << '\n';
        }

        void send(const char * message) {
            std::cout << "== [Direct] Sending..." << '\n';

            zmq::message_t request(strlen(message));
            memcpy(request.data(), message, strlen(message));
            this->socket->send(request);

            std::cout << "== [Direct] Sent: " << message << '\n';
        }

        std::string recv(){
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

    	void shutdown() {
    	    std::cout << "Shutting down socket and context\n";
    	    zmq_close(this->socket);
    	    zmq_ctx_destroy(this->context);
    	}

    	void handle_messaging() {
    	    CommandType command = INFORMATION_TYPE;
    	    int phase = 0;
    	    std::string node_id = "00-00-00-00-00";
    	    std::string data = "OK";
    	    unsigned int nonce = 998;
    	    Message message(node_id, command, phase, data, nonce);

    	    while (1) {
        		std::string req = this->recv();
        		if (req != "") {
        		    data = "OK";
        		    Message eof_message(node_id, command, phase, data, nonce);
        		    Message msg_req(req);
        		    if (msg_req.get_data() == "SHUTDOWN") {
        			this->shutdown();
        			this->send(&eof_message);
        			exit(0);
        			//break;
        		    } else {
        			this->send(&message);
        		    }
        		}
        		sleep(2);
    	    }
    	}
};

class Bluetooth : public Connection {
    public:
        void whatami() {
            std::cout << "I am a BLE implementation." << '\n';
        }

        void send(Message * msg) {
            msg->get_pack();
        }

        void send(const char * message) {
            std::cout << "[Direct] Sent: " << message << '\n';
        }

    	std::string recv(){

            return "Message";
    	}

    	void shutdown() {

    	}
};

class LoRa : public Connection {
    private:

    public:
        LoRa() {}
        LoRa(Options *options){
            switch (options->operation) {
                case SERVER:
                    std::cout << "Serving..." << "\n\n";
                    break;
                case CLIENT:
                    std::cout << "Connecting..." << "\n\n";
                    break;
            }
        }

        void whatami() {
            std::cout << "I am a LoRa implementation." << '\n';
        }

        void send(Message * msg) {
            msg->get_pack();

        }

        void send(const char * message) {
            std::cout << "[Direct] Sent: " << message << '\n';
        }

    	std::string recv(){

            return "Message";
    	}

    	void shutdown() {

    	}
};

class Websocket : public Connection {
    public:
        void whatami() {
            std::cout << "I am a Websocket implementation." << '\n';
        }

        void send(Message * msg) {
            msg->get_pack();
        }

        void send(const char * message) {
            std::cout << "[Direct] Sent: " << message << '\n';
        }

    	std::string recv(){

            return "Message";
    	}

    	void shutdown() {

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
        case NONE:
            return NULL;
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
        case NONE:
            return NULL;
            break;
    }
    return NULL;
}

#endif
