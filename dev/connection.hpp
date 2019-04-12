#ifndef CONNECTION_HPP
#define CONNECTION_HPP

#include <unistd.h>
#include <vector>
#include <fstream>

#include <sys/socket.h>
#include <stdlib.h>
#include <netinet/in.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <arpa/inet.h>

#include "zmq.hpp"

#include "utility.hpp"
#include "message.hpp"

// TODO:
// * Drop connections if the connected node does not match the intended device
// * Maintain key and nonce state for connections

class direct_monitor_t : public zmq::monitor_t {
    public:
        bool connected = false;
        bool disconnected = false;

        void on_event_connected(const zmq_event_t &, const char *) ZMQ_OVERRIDE {
            this->connected = true;
        }

        void on_event_closed(const zmq_event_t &, const char *) ZMQ_OVERRIDE {
            this->disconnected = true;
        }

        void on_event_disconnected(const zmq_event_t &, const char *) ZMQ_OVERRIDE {
            this->disconnected = true;
        }
};

class Connection {
    protected:
    	bool active;
    	bool instantiate_connection;
    	DeviceOperation operation;

    	std::string peer_name;
    	std::string pipe_name;
    	std::fstream pipe;
    	unsigned long message_sequence;

        SystemClock update_clock;

        bool worker_active = false;
        bool response_needed = false;
        std::thread worker_thread;
        std::mutex worker_mutex;
        std::condition_variable worker_conditional;

    public:
        virtual void whatami() = 0;
    	virtual std::string get_type() = 0;
    	virtual void spawn() = 0;
    	virtual void worker() = 0;
    	virtual std::string recv(int flag = 0) = 0;
    	virtual void send(Message *) = 0;
    	virtual void send(const char * message) = 0;
    	virtual void shutdown() = 0;

    	bool get_status() {
    	    return this->active;
    	}

    	bool get_worker_status() {
    	    return this->worker_active;
    	}

        std::string get_peer_name() {
            return this->peer_name;
        }

        std::string get_pipe_name() {
            return this->pipe_name;
        }

        SystemClock get_update_clock() {
            return this->update_clock;
        }

    	bool create_pipe() {
    	    if (this->peer_name == "") {
    		return false;
    	    }

    	    std::string filename = "./tmp/" + this->peer_name + ".pipe";

    	    this->pipe_name = filename;
    	    this->pipe.open(filename, std::fstream::in | std::fstream::out | std::fstream::trunc);

    	    if( this->pipe.fail() ){
    		this->pipe.close();
    		return false;
    	    }

    	    return true;
    	}

    	bool write_to_pipe(std::string message) {
    	    if ( !this->pipe.good() ) {
    		return false;
    	    }

    	    std::cout << "== [Connection] Writing \"" << message << "\" to pipe" << '\n';
    	    this->pipe.clear();
    	    this->pipe.seekp(0);
    	    this->pipe << message << std::endl;
    	    this->pipe.flush();
    	    return true;
    	}

    	std::string read_from_pipe() {
    	    std::string raw_message = "";

            if ( !this->pipe.good() ) {
                std::cout << "== [Connection] Pipe file is not good" << '\n';
                return raw_message;
            }
    	    this->pipe.clear();
    	    this->pipe.seekg(0);

            if (this->pipe.eof()) {
                std::cout << "== [Connection] Pipe file is at the EOF" << '\n';
                return raw_message;
            }

            std::getline( this->pipe, raw_message );
            this->pipe.clear();

            std::cout << "== [Connection] Sending " << raw_message << '\n';


            std::ofstream clear_file(pipe_name, std::ios::out | std::ios::trunc);
            clear_file.close();

            return raw_message;
        }

        void response_ready(std::string id) {
            if (this->peer_name != id) {
                std::cout << "== [Connection] Response was not for this peer" << '\n';
                return;
            }

            this->response_needed = false;
            std::unique_lock<std::mutex> thread_lock(this->worker_mutex);
            worker_conditional.notify_one();
            thread_lock.unlock();
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
        Direct(struct Options *options) {
            std::cout << "== [Connection] Creating direct instance.\n";

            this->port = options->port;
            this->peer_name = options->peer_name;
            this->peer_addr = options->peer_addr;
            this->peer_port = options->peer_port;
            this->control = options->is_control;
            this->operation = options->operation;

            this->update_clock = get_system_clock();

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
                    case NO_OPER: {
                        std::cout << "ERROR DEVICE OPERATION NEEDED" << "\n";
                        exit(0);
                        break;
                    }
        		}

        		return;

    	    }

            this->spawn();

            std::unique_lock<std::mutex> thread_lock(worker_mutex);
            this->worker_conditional.wait(thread_lock, [this]{return this->worker_active;});
            thread_lock.unlock();

            // CHECK WORKER ACTIVE FROM CALLING FUNCTION

        }

        void whatami() {
            std::cout << "I am a Direct implementation." << '\n';
        }

        std::string get_type() {
            return "WiFi";
        }

        void spawn() {
            std::cout << "== [Connection] Spawning DIRECT_TYPE connection thread\n";
            this->worker_thread = std::thread(&Direct::worker, this);
        }

        void worker() {
            this->worker_active = true;
            this->create_pipe();

            // Create appriopiate network socket
            this->context = new zmq::context_t(1);

            switch (this->operation) {
                case ROOT: {
                    std::cout << "== [Connection] Setting up REP socket on port " << this->port << "\n";
                    this->setup_rep_socket(this->port);
                    // handle_messaging();
                    break;
                }
                case BRANCH: {
                    break;
                }
                case LEAF: {
                    std::cout << "== [Connection] Connecting REQ socket to " << this->peer_addr << ":" << this->peer_port << "\n";
                    this->setup_req_socket(this->peer_addr, this->peer_port);

                    break;
                }
                case NO_OPER: {
                    std::cout << "ERROR DEVICE OPERATION NEEDED" << "\n";
                    exit(0);
                    break;
                }
            }

            this->worker_active = true;

            // Notify the calling thread that the connection worker is ready
            std::unique_lock<std::mutex> thread_lock(this->worker_mutex);
            worker_conditional.notify_one();
            thread_lock.unlock();

            unsigned int run = 0;

            do {
                std::string request = "";
                // Receive message
                request = this->recv(0);
                this->write_to_pipe(request);

                // std::this_thread::sleep_for(std::chrono::milliseconds(500));

                this->response_needed = true;

                // Wait for message from pipe then send
                std::unique_lock<std::mutex> thread_lock(worker_mutex);
                this->worker_conditional.wait(thread_lock, [this]{return !this->response_needed;});
                thread_lock.unlock();

                std::string response = this->read_from_pipe();
                this->send(response.c_str());

                // Message response("1", this->peer_name, QUERY_TYPE, 1, "Message Response", run);
                // this->send(&response);

                run++;
                std::this_thread::sleep_for(std::chrono::nanoseconds(1000));
            } while(true);

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

    	void send(class Message * message) {
    	    std::string msg_pack = message->get_pack();
    	    zmq::message_t request(msg_pack.size());
    	    memcpy(request.data(), msg_pack.c_str(), msg_pack.size());

    	    this->socket->send(request);
            this->message_sequence++;

    	    std::cout << "== [Connection] Sent\n";
    	}

    	void send(const char * message) {
    	    zmq::message_t request(strlen(message));
    	    memcpy(request.data(), message, strlen(message));
    	    this->socket->send(request);
            this->message_sequence++;

    	    std::cout << "== [Connection] Sent\n";
    	}

    	std::string recv(int flag){
    	    zmq::message_t message;
    	    this->socket->recv(&message, flag);
    	    std::string request = std::string(static_cast<char *>(message.data()), message.size());

            if (request != "") {
                this->update_clock = get_system_clock();
                this->message_sequence++;
            }

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
	StreamBridge(struct Options *options) {
	    std::cout << "Creating StreamBridge instance.\n";

	    this->port = options->port;
	    this->peer_addr = options->peer_addr;
	    this->peer_port = options->peer_port;
	    this->control = options->is_control;
	    this->operation = options->operation;

	    // this->worker_active = false;

	    if (options->is_control) {
		std::cout << "== [StreamBridge] creating control socket\n";

		this->context = zmq_ctx_new();// add 1 or no

		switch (options->operation) {
		    case ROOT: {
				    std::cout << "== [StreamBridge] setting up stream socket on port " << options->port << "\n";
				    this->socket = zmq_socket(this->context, ZMQ_STREAM);
				    setup_streambridge_socket(options->port);
				    break;
			       }
		    case BRANCH: {
				     break;
				 }
		    case LEAF: {
				    //std::cout << "== [StreamBridge] connecting stream socket to " << options->peer_addr << ":" << options->peer_port << "\n";
				    //this->socket = zmq_socket(this->context, ZMQ_STREAM);
				    //setup_streambridge_socket(options->port);
				    //this->setup_req_socket(options->peer_addr, options->peer_port);
				    //break;
			       }
                   case NO_OPER: {
                       std::cout << "ERROR DEVICE OPERATION NEEDED" << "\n";
                       exit(0);
                       break;
                   }
		}
		return;
	    }

	    this->spawn();

	    std::unique_lock<std::mutex> thread_lock(worker_mutex);
	    this->worker_conditional.wait(thread_lock, [this]{return this->worker_active;});
	    thread_lock.unlock();
	}

	void whatami() {
	    std::cout << "I am a StreamBridge implementation." << '\n';
	}

    std::string get_type() {
        return "WiFi";
    }

	void spawn() {
	    std::cout << "== [StreamBridge] Spawning STREAMBRIDGE_TYPE connection thread\n";
	    this->worker_thread = std::thread(&StreamBridge::worker, this);
	}

	void worker() {
	    this->worker_active = true;
	    this->create_pipe();

	    this->context = zmq_ctx_new();// add 1 or no

	    switch (this->operation) {
		case ROOT: {
				std::cout << "== [StreamBridge] setting up stream socket on port " << this->port << "\n";
				this->socket = zmq_socket(this->context, ZMQ_STREAM);
				setup_streambridge_socket(this->port);
				break;
			   }
		case BRANCH: {
				 break;
			     }
		case LEAF: {
				//std::cout << "== [StreamBridge] connecting stream socket to " << options->peer_addr << ":" << options->peer_port << "\n";
				//this->socket = zmq_socket(this->context, ZMQ_STREAM);
				//setup_streambridge_socket(options->port);
				//this->setup_req_socket(options->peer_addr, options->peer_port);
				//break;
			   }
               case NO_OPER: {
                   std::cout << "ERROR DEVICE OPERATION NEEDED" << "\n";
                   exit(0);
                   break;
               }
	    }

	    this->worker_active = true;

	    // Notify the calling thread that the connection worker is ready
	    std::unique_lock<std::mutex> thread_lock(this->worker_mutex);
	    worker_conditional.notify_one();
	    thread_lock.unlock();

	    unsigned int run = 0;

	    do {
		std::string request = "";
		// Receive message
		request = this->recv(0);
		this->write_to_pipe(request);

		std::this_thread::sleep_for(std::chrono::milliseconds(500));

		// Wait for message from pipe then send

		Message response("1", this->peer_name, QUERY_TYPE, 1, "Message Response", run);
		this->send(&response);

		run++;
		std::this_thread::sleep_for(std::chrono::nanoseconds(1000));
	    } while(true);
	}

	void setup_streambridge_socket(std::string port) {
	    std::cout << "[Control] Setting up streambridge socket on port " << port << "... PID: " << getpid() << "\n\n";
	    this->instantiate_connection = true;
	    std::string conn_data = "tcp://*:" + port;
	    zmq_bind(this->socket, conn_data.c_str());
	}

	std::string recv(int flag){
        if (flag) {}
	    //do {
	    char buffer[512];
	    memset(buffer, '\0', 512);

	    // Receive 3 times, first is ID, second is nothing, third is message
	    this->id_size = zmq_recv(this->socket, this->id, 256, 0);
	    size_t msg_size = zmq_recv(this->socket, buffer, 14, 0);
	    memset(buffer, '\0', 512);
	    msg_size = zmq_recv(this->socket, buffer, 14, 0);
	    memset(buffer, '\0', 512);
	    msg_size = zmq_recv(this->socket, buffer, 14, 0);
	    std::cout << "Received: " << buffer << "\n";

	    return buffer;
	    //} while ( true );
	}

	void send(class Message * msg) {
	    std::cout << "[StreamBridge] Sending..." << '\n';
	    std::string msg_pack = msg->get_pack();
	    zmq_send(this->socket, this->id, this->id_size, ZMQ_SNDMORE);
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

	int socket, connection;
	struct sockaddr_in address;
	int opt = 1;
	int addrlen = sizeof(address);

    public:
	TCP() {}
	TCP(struct Options *options) {
	    std::cout << "[TCP] Creating TCP instance.\n";

	    this->port = options->port;
	    this->peer_addr = options->peer_addr;
	    this->peer_port = options->peer_port;
	    this->control = options->is_control;
	    this->operation = options->operation;

	    // this->worker_active = false;

	    if (options->is_control) {
		std::cout << "== [TCP] creating control socket\n";

		switch (options->operation) {
		    case ROOT: {
				   std::cout << "== [TCP] setting up tcp socket on port " << options->port << "\n";
				   this->setup_tcp_socket(options->port);
				   break;
			       }
		    case BRANCH: {
				     break;
				 }
		    case LEAF: {
				   std::cout << "== [TCP] connecting tcp client socket to " << options->peer_addr << ":" << options->peer_port << "\n";
				   this->setup_tcp_connection(options->peer_addr, options->peer_port);
				   break;
			       }
           case NO_OPER: {
               std::cout << "ERROR DEVICE OPERATION NEEDED" << "\n";
               exit(0);
               break;
           }
		}

		this->spawn();

		std::unique_lock<std::mutex> thread_lock(worker_mutex);
		this->worker_conditional.wait(thread_lock, [this]{return this->worker_active;});
		thread_lock.unlock();

		return;
	    }
	}

	void whatami() {
	    std::cout << "I am a TCP implementation." << '\n';
	}

    std::string get_type() {
        return "WiFi";
    }

	void spawn() {
	    std::cout << "== [TCP] Spawning TCP_TYPE connection thread\n";
	    this->worker_thread = std::thread(&TCP::worker, this);
	}

	void worker() {
	    this->worker_active = true;
	    this->create_pipe();

	    switch (this->operation) {
		case ROOT: {
		    std::cout << "== [TCP] Setting up tcp socket on port " << this->port << "\n";
		    this->setup_tcp_socket(this->port);
		    // handle_messaging();
		    break;
		}
		case BRANCH: {
		    break;
		}
		case LEAF: {
		    std::cout << "== [TCP] Connecting tcp client socket to " << this->peer_addr << ":" << this->peer_port << "\n";
		    this->setup_tcp_connection(this->peer_addr, this->peer_port);

		    break;
		}
        case NO_OPER: {
            std::cout << "ERROR DEVICE OPERATION NEEDED" << "\n";
            exit(0);
            break;
        }
	    }

	    this->worker_active = true;

	    // Notify the calling thread that the connection worker is ready
	    std::unique_lock<std::mutex> thread_lock(this->worker_mutex);
	    worker_conditional.notify_one();
	    thread_lock.unlock();

	    unsigned int run = 0;

	    do {
		std::string request = "";
		// Receive message
		request = this->recv(0);
		this->write_to_pipe(request);

		std::this_thread::sleep_for(std::chrono::milliseconds(500));

		// Wait for message from pipe then send

		Message response("1", this->peer_name, QUERY_TYPE, 1, "Message Response", run);
		this->send(&response);

		run++;
		std::this_thread::sleep_for(std::chrono::nanoseconds(1000));
	    } while(true);
	}

	void setup_tcp_socket(std::string port) {
	    // Creating socket file descriptor
	    if ((this->socket = ::socket(AF_INET, SOCK_STREAM, 0)) < 0) {
		std::cout << "Socket failed\n";
		return;
	    }

	    if (setsockopt(this->socket, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &this->opt, sizeof(this->opt))) {
		std::cout << "Setsockopt failed\n";
		return;
	    }
	    this->address.sin_family = AF_INET;
	    this->address.sin_addr.s_addr = INADDR_ANY;
	    int PORT = std::stoi(port);
	    this->address.sin_port = htons( PORT );

	    if (bind(this->socket, (struct sockaddr *)&this->address, sizeof(this->address))<0) {
		std::cout << "Bind failed\n";
		return;
	    }
	    if (listen(this->socket, 30) < 0) {
		std::cout << "Listen failed\n";
		return;
	    }
	    if ((this->connection = accept(this->socket, (struct sockaddr *)&this->address, (socklen_t*)&this->addrlen))<0) {
		std::cout << "Socket failed\n";
	    }
	}

	void setup_tcp_connection(std::string peer_addr, std::string peer_port) {
	    if ((this->connection = ::socket(AF_INET, SOCK_STREAM, 0)) < 0) {
		printf("Socket creation error\n");
		return;
	    }

	    memset(&this->address, '0', sizeof(this->address));

	    this->address.sin_family = AF_INET;
	    int PORT = std::stoi(peer_port);
	    this->address.sin_port = htons(PORT);

	    // Convert IPv4 and IPv6 addresses from text to binary form
	    if (inet_pton(AF_INET, peer_addr.c_str(), &(this->address.sin_addr)) <= 0) {
		printf("\nInvalid address/ Address not supported \n");
		return;
	    }

	    if (connect(this->connection, (struct sockaddr *)&(this->address), sizeof(this->address)) < 0) {
		printf("\nConnection Failed \n");
		return;
	    }
	}

	std::string recv(int flag){
        if (flag) {}
	    char buffer[1024];
	    memset(buffer, '\0', 1024);
	    int valread = read(this->connection, buffer, 1024);
	    printf("Received: (%d) %s\n", valread, buffer);

	    return buffer;
	}

	void send(Message * msg) {
	    std::cout << "[TCP] Sending...\n";
	    std::string msg_pack = msg->get_pack();
	    ssize_t bytes = ::send(this->connection, msg_pack.c_str(), strlen(msg_pack.c_str()), 0);
	    std::cout << "[TCP] Sent: (" << (int)bytes << ") " << msg_pack << '\n';
	}

	void send(const char * message) {
	    std::cout << "[TCP] Sending...\n";
	    ssize_t bytes = ::send(this->connection, message, strlen(message), 0);
	    std::cout << "[TCP] Sent: (" << (int)bytes << ") " << message << '\n';
	}

	void shutdown() {
	    close(this->connection);
	    close(this->socket);
	}

};


class Bluetooth : public Connection {
    public:
	void whatami() {
	    std::cout << "I am a BLE implementation." << '\n';
	}

    std::string get_type() {
        return "BLE";
    }

	void spawn() {

	}

	void worker() {

	}

	void send(class Message * msg) {
	    msg->get_pack();
	}

	void send(const char * message) {
	    std::cout << "[Connection] Sent: " << message << '\n';
	}

	std::string recv(int flag){
        if (flag) {}

	    return "Message";
	}

	void shutdown() {

	}
};

class LoRa : public Connection {
    public:
	LoRa() {}
	LoRa(struct Options *options){
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
        case NO_OPER:
            std::cout << "ERROR DEVICE OPERATION NEEDED" << "\n";
            exit(0);
            break;
	    }
	}

	void whatami() {
	    std::cout << "I am a LoRa implementation." << '\n';
	}

    std::string get_type() {
        return "LoRa";
    }

	void spawn() {

	}

	void worker() {

	}

	void send(class Message * msg) {
	    msg->get_pack();

	}

	void send(const char * message) {
	    std::cout << "[Connection] Sent: " << message << '\n';
	}

	std::string recv(int flag){
        if (flag) {}

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
	case TCP_TYPE:
	    return new TCP;
	    break;
	case STREAMBRIDGE_TYPE:
	    return new StreamBridge;
	    break;
	case NO_TECH:
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
	case TCP_TYPE:
	    return new TCP(options);
	    break;
	case STREAMBRIDGE_TYPE:
	    return new StreamBridge(options);
	    break;
	case NO_TECH:
	    return NULL;
	    break;
    }
    return NULL;
}

#endif
