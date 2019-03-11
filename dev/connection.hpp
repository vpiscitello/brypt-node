#ifndef CONNECTION_HPP
#define CONNECTION_HPP

#include <unistd.h>
#include <vector>
#include <fstream>

#include <thread>

#include <sys/socket.h>
#include <stdlib.h>
#include <netinet/in.h>
#include <string.h>

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

        bool worker_active;
        std::thread worker_thread;

    public:
        virtual void whatami() = 0;
        virtual void spawn() = 0;
        virtual void worker() = 0;
        virtual std::string recv(int flag = 0) = 0;
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
            this->pipe.open(filename, std::ios::app);

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

            // this->pipe.write( message.c_str(), message.size() );
            std::cout << "== [Connection] Writing \"" << message << "\" to pipe" << '\n';
            this->pipe << message;
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
        std::string peer_addr;
        std::string peer_port;

        zmq::context_t *context;
        zmq::socket_t *socket;
        zmq::pollitem_t item;

    public:
        Direct() {}
        Direct(Options *options) {
            std::cout << "== [Connection] Creating direct instance.\n";

            this->port = options->port;
            this->peer_name = options->peer_name;
            this->peer_addr = options->peer_addr;
            this->peer_port = options->peer_port;
            this->control = options->is_control;

            this->worker_active = false;

    	    if (options->is_control) {
        		std::cout << "== [Connection] Creating control socket\n";

        		this->context = new zmq::context_t(1);

        		switch (options->operation) {
                    case ROOT: {
                        std::cout << "== [Connection] Setting up REP socket on port " << options->port << "\n";
                        this->setup_rep_socket(options->port);
                        break;
                    }
                    case BRANCH: {
                        break;
                    }
                    case LEAF: {
                        std::cout << "== [Connection] Connecting REQ socket to " << options->peer_addr << ":" << options->peer_port << "\n";
                        this->setup_req_socket(options->peer_addr, options->peer_port);
                        break;
                    }
        		}

        		return;

    	    }

            this->spawn();

    	    // switch (this->c_pid) {
        	// 	case -1: {
            //         std::cout << "Error creating child process\n";
            //         return;
            //     }
            //
        	// 	case 0: {
            //         sleep(1);
            //
            //         try {
            //             std::cout << "== [Connection] Connection thread PID " << getpid() << "\n";
            //
            //             this->context = new zmq::context_t(1);
            //
            //             switch (options->operation) {
            //                 case ROOT: {
            //                     std::cout << "== [Connection] Setting up REP socket on port " << options->port << "\n";
            //                     this->setup_rep_socket(options->port);
            //
            //                     // handle_messaging();
            //                     break;
            //                 }
            //                 case BRANCH: {
            //                     break;
            //                 }
            //                 case LEAF: {
            //                     std::cout << "== [Connection] Connecting REQ socket to " << options->peer_addr << ":" << options->peer_port << "\n";
            //                     this->setup_req_socket(options->peer_addr, options->peer_port);
            //
            //                     break;
            //                 }
            //             }
            //         } catch(...) {
            //             std::cout << "Error creating child process\n";
            //         }
            //
            //         return;
            //     }
            //
        	// 	default: {
            //         return;
            //     }
    	    // }

        }

        void whatami() {
            std::cout << "I am a Direct implementation." << '\n';
        }

        void spawn() {
            std::cout << "== [Connection] Spawning DIRECT_TYPE connection thread\n";
            this->worker_thread = std::thread(&Direct::worker, this);
        }

        void worker() {
            this->worker_active = true;
            this->create_pipe();
            unsigned int run = 0;
            while (true) {
                this->write_to_pipe(std::to_string(run) + " Here is some work!");
                run++;
                sleep(1);
            }
        }

        void setup_rep_socket(std::string port) {
            this->socket = new zmq::socket_t(*this->context, ZMQ_REP);
            this->item.socket = *this->socket;
            this->item.events = ZMQ_POLLIN;
            this->instantiate_connection = true;
            this->socket->bind("tcp://*:" + port);
        }

        void setup_req_socket(std::string addr, std::string port) {
            this->socket = new zmq::socket_t(*this->context, ZMQ_REQ);
            this->item.socket = *this->socket;
            this->item.events = ZMQ_POLLIN;
            this->socket->connect("tcp://" + addr + ":" + port);
            this->instantiate_connection = false;
        }

    	void send(Message * message) {
    	    std::string msg_pack = message->get_pack();
    	    zmq::message_t request(msg_pack.size());
    	    memcpy(request.data(), msg_pack.c_str(), msg_pack.size());

    	    this->socket->send(request);

            std::cout << "== [Connection] Sent\n";
        }

        void send(const char * message) {
            zmq::message_t request(strlen(message));
            memcpy(request.data(), message, strlen(message));
            this->socket->send(request);

            std::cout << "== [Connection] Sent\n";
        }

        std::string recv(int flag){
            zmq::message_t message;
            this->socket->recv(&message, flag);
            std::string request = std::string(static_cast<char *>(message.data()), message.size());
            return request;
        }

    	void shutdown() {
    	    std::cout << "Shutting down socket and context\n";
    	    zmq_close(this->socket);
    	    zmq_ctx_destroy(this->context);
    	}

    	void handle_messaging() {

    	}
};

class StreamBridge : public Connection {
    private:
        bool control;

        std::string port;
        std::string peer_addr;
        std::string peer_port;

    	void *context;
    	void *socket;

    	uint8_t id[256];
    	size_t id_size = 256;

    public:
        StreamBridge() {}
        StreamBridge(Options *options) {
            std::cout << "Creating StreamBridge instance.\n";

            this->port = options->port;
            this->peer_addr = options->peer_addr;
            this->peer_port = options->peer_port;
            this->control = options->is_control;

            if (options->is_control) {
                std::cout << "[StreamBridge] Creating control socket\n";
                this->context = zmq_ctx_new();// add 1 or no

        		//TODO Implement this in the switch statement
        		this->socket = zmq_socket(this->context, ZMQ_STREAM);
        		setup_stream_socket(options->port);
                return;
            }

	    // Add forking/threading portion
        }

        void whatami() {
            std::cout << "I am a StreamBridge implementation." << '\n';
        }

        void spawn() {

        }

        void worker() {

        }

    	void setup_stream_socket(std::string port) {
    	    std::cout << "[Control] Setting up connection on port " << port << "... PID: " << getpid() << "\n\n";
    	    this->instantiate_connection = true;
    	    std::string conn_data = "tcp://*:" + port;
    	    zmq_bind(this->socket, conn_data.c_str());
    	}

        std::string recv(int flag){
            //do {
    		char buffer[512];
    		memset(buffer, '\0', 512);

    		//TODO CHANGE THE 14
    		// Receive 3 times, first is ID, second is nothing, third is message
    		size_t msg_size = zmq_recv(this->socket, buffer, 14, 0);
    		memset(buffer, '\0', 512);
    		msg_size = zmq_recv(this->socket, buffer, 14, 0);
    		memset(buffer, '\0', 512);
    		msg_size = zmq_recv(this->socket, buffer, 14, 0);
    		std::cout << "Received: " << buffer << "\n";

    		return buffer;
            //} while ( true );
    	}

    	void send(Message * msg) {
    	    std::cout << "[StreamBridge] Sending..." << '\n';
    	    std::string msg_pack = msg->get_pack();
    	    zmq_send(this->socket, id, id_size, ZMQ_SNDMORE);
    	    zmq_send(this->socket, msg_pack.c_str(), strlen(msg_pack.c_str()), ZMQ_SNDMORE);
    	    std::cout << "[StreamBridge] Sent: (" << strlen(msg_pack.c_str()) << ") " << msg_pack << '\n';
        }

        void send(const char * message) {
    	    std::cout << "[StreamBridge] Sending..." << '\n';
    	    zmq_send(this->socket, id, id_size, ZMQ_SNDMORE);
    	    zmq_send(this->socket, message, strlen(message), ZMQ_SNDMORE);
    	    std::cout << "[StreamBridge] Sent: (" << strlen(message) << ") " << message << '\n';
        }

    	void shutdown() {
	    // possibly do the send 0 length message thing
    	    //std::cout << "Shutting down socket and context\n";
    	    //zmq_close(this->socket);
    	    //zmq_ctx_destroy(this->context);
    	}

};


class TCP : public Connection {
    private:
        bool control;

        std::string port;
        std::string peer_addr;
        std::string peer_port;

    	int server_fd, new_socket;
    	struct sockaddr_in address;
    	int opt = 1;
    	int addrlen = sizeof(address);

    public:
        TCP() {}
        TCP(Options *options) {
            std::cout << "[TCP] Creating TCP instance.\n";

            this->port = options->port;
            this->peer_addr = options->peer_addr;
            this->peer_port = options->peer_port;
            this->control = options->is_control;

            if (options->is_control) {
                std::cout << "[TCP] Creating control socket\n";

        		std::cout << "[TCP] Setting up connection on port " << options->port << "... PID: " << getpid() << "\n\n";
        		setup_tcp_socket(options->port);
                return;
            }

        }

        void whatami() {
            std::cout << "I am a TCP implementation." << '\n';
        }

        void spawn() {

        }

        void worker() {

        }

    	void setup_tcp_socket(std::string port) {
    	    // Creating socket file descriptor
    	    if ((this->server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
    		std::cout << "Socket failed\n";
    		return;
    	    }

    	    if (setsockopt(this->server_fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &this->opt, sizeof(this->opt))) {
    		std::cout << "Setsockopt failed\n";
    		return;
    	    }
    	    this->address.sin_family = AF_INET;
    	    this->address.sin_addr.s_addr = INADDR_ANY;
    	    int PORT = std::stoi(port);
    	    this->address.sin_port = htons( PORT );

    	    if (bind(this->server_fd, (struct sockaddr *)&this->address, sizeof(this->address))<0) {
    		std::cout << "Bind failed\n";
    		return;
    	    }
    	    if (listen(this->server_fd, 30) < 0) {
    		std::cout << "Listen failed\n";
    		return;
    	    }
    	    if ((this->new_socket = accept(this->server_fd, (struct sockaddr *)&this->address, (socklen_t*)&this->addrlen))<0) {
    		std::cout << "Socket failed\n";
    	    }
            }

            std::string recv(int flag){
    	    char buffer[1024];
    	    memset(buffer, '\0', 1024);
    	    int valread = read(this->new_socket , buffer, 1024);
    	    printf("Received: (%d) %s\n", valread, buffer);

    	    return buffer;
        	}

        	void send(Message * msg) {
    	    std::cout << "[TCP] Sending...\n";
        	    std::string msg_pack = msg->get_pack();
    	    ssize_t bytes = ::send(new_socket, msg_pack.c_str(), strlen(msg_pack.c_str()), 0);
        	    std::cout << "[TCP] Sent: (" << (int)bytes << ") " << msg_pack << '\n';
            }

            void send(const char * message) {
    	    std::cout << "[TCP] Sending...\n";
    	    ssize_t bytes = ::send(new_socket, message, strlen(message), 0);
        	    std::cout << "[TCP] Sent: (" << (int)bytes << ") " << message << '\n';
    	}

    	void shutdown() {
	    // possibly do the send 0 length message thing
    	//    //std::cout << "Shutting down socket and context\n";
    	//    //zmq_close(this->socket);
    	//    //zmq_ctx_destroy(this->context);
    	}

    	//void handle_messaging() {
        //    CommandType command = INFORMATION_TYPE;
        //    int phase = 0;
        //    std::string node_id = "00-00-00-00-00";
        //    std::string data = "OK";
        //    unsigned int nonce = 998;
        //    Message message(node_id, command, phase, data, nonce);

        //    while (1) {
        //        std::string req = this->recv();
        //        if (req != "") {
        //            data = "OK";
        //            Message eof_message(node_id, command, phase, data, nonce);
        //            Message msg_req(req);
        //            if (msg_req.get_data() == "SHUTDOWN") {
        //                this->shutdown();
        //                this->send(&eof_message);
        //                exit(0);
        //                //break;
        //            } else {
        //                this->send(&message);
        //            }
        //        }
        //        sleep(2);
        //    }
	//}
};


class Bluetooth : public Connection {
    public:
        void whatami() {
            std::cout << "I am a BLE implementation." << '\n';
        }

        void spawn() {

        }

        void worker() {

        }

        void send(Message * msg) {
            msg->get_pack();
        }

        void send(const char * message) {
            std::cout << "[Connection] Sent: " << message << '\n';
        }

    	std::string recv(int flag){

            return "Message";
    	}

    	void shutdown() {

    	}
};

class LoRa : public Connection {
    public:
        LoRa() {}
        LoRa(Options *options){
            switch (options->operation) {
                case ROOT:
                    std::cout << "Serving..." << "\n";
                    break;
                case BRANCH:
                    std::cout << "Serving..." << "\n";
                    break;
                case LEAF:
                    std::cout << "Connecting..." << "\n";
                    break;
            }
        }

        void whatami() {
            std::cout << "I am a LoRa implementation." << '\n';
        }

        void spawn() {

        }

        void worker() {

        }

        void send(Message * msg) {
            msg->get_pack();

        }

        void send(const char * message) {
            std::cout << "[Connection] Sent: " << message << '\n';
        }

    	std::string recv(int flag){

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

        void spawn() {

        }

        void worker() {

        }

        void send(Message * msg) {
            msg->get_pack();
        }

        void send(const char * message) {
            std::cout << "[Connection] Sent: " << message << '\n';
        }

    	std::string recv(int flag){

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
        case TCP_TYPE:
            return new TCP;
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
        case TCP_TYPE:
            return new TCP(options);
            break;
        case NONE:
            return NULL;
            break;
    }
    return NULL;
}

#endif
