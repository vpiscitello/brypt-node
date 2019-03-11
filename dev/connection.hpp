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
            std::cout << "== [Connection] Creating direct instance.\n";

            this->port = options->port;
            this->peer_IP = options->peer_IP;
            this->peer_port = options->peer_port;
            this->control = options->is_control;

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
			std::cout << "== [Connection] Connecting REQ socket to " << options->peer_IP << ":" << options->peer_port << "\n";
			this->setup_req_socket(options->peer_IP, options->peer_port);
			break;
		    }
		}

		return;
	    }

	    std::cout << "== [Connection] Creating DIRECT connection thread\n";
	    this->c_pid = fork();

            switch (this->c_pid) {
                case -1: {
                    std::cout << "Error creating child process\n";
                    return;
                }
                case 0: {
		    sleep(1);
		    std::cout << "== [Connection] Connection thread PID " << getpid() << "\n";
		    this->context = new zmq::context_t(1);

		    switch (options->operation) {
			case ROOT: {
			    std::cout << "== [Connection] Setting up REP socket on port " << options->port << "\n";
			    // this->setup_rep_socket(options->port);
			    // handle_messaging();
			    break;
			}
			case BRANCH: {
			    break;
			}
			case LEAF: {
			    std::cout << "== [Connection] Connecting REQ socket to " << options->peer_IP << ":" << options->peer_port << "\n";
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

//class StreamBridge : public Connection {
//    private:
//        bool control;
//
//        std::string port;
//        std::string peer_IP;
//        std::string peer_port;
//
//	void *context;
//	void *socket;
//
//	uint8_t id[256];
//	size_t id_size = 256;
//
//    public:
//        StreamBridge() {}
//        StreamBridge(Options *options) {
//            std::cout << "Creating StreamBridge instance.\n";
//
//            this->port = options->port;
//            this->peer_IP = options->peer_IP;
//            this->peer_port = options->peer_port;
//            this->control = options->is_control;
//
//            if (options->is_control) {
//                std::cout << "[StreamBridge] Creating control socket\n";
//                this->context = zmq_ctx_new();// add 1 or no
//
//                switch (options->operation) {
//                    case SERVER: {
//                        this->socket = zmq_socket(this->context, ZMQ_STREAM);
//                        std::cout << "[Control] Setting up connection on port " << options->port << "... PID: " << getpid() << "\n\n";
//                        this->instantiate_connection = true;
//                        zmq_bind(this->socket, "tcp://*:" + options->port);
//                        break;
//                    }
//                    case CLIENT: {
//                        ////this->socket = new zmq::socket_t(*this->context, ZMQ_STREAM);
//                        //this->socket = new zmq::socket_t(*this->context, ZMQ_REQ);
//                        //this->item.socket = *this->socket;
//                        //this->item.events = ZMQ_POLLIN;
//                        //std::cout << "[StreamBridge Control Client] Connecting to: " << options->peer_IP << ":" << options->peer_port << "\n\n";
//                        //this->socket->connect("tcp://" + options->peer_IP + ":" + options->peer_port);
//                        //this->instantiate_connection = false;
//                        break;
//                    }
//                }
//                return;
//            }
//
//            //std::cout << "[StreamBridge] Creating normal socket\n";
//            //this->c_pid = fork();
//
//            //switch (this->c_pid) {
//            //    case -1: {
//            //        std::cout << "Error creating child process\n";
//            //        return;
//            //    }
//            //    case 0: {
//            //        sleep(1);
//            //        std::cout << "[StreamBridge] The child id is " << getpid() << "\n";
//            //        this->context = new zmq::context_t(1);
//
//            //        switch (options->operation) {
//            //            case SERVER: {
//            //                //this->socket = new zmq::socket_t(*this->context, ZMQ_STREAM);
//            //                this->socket = new zmq::socket_t(*this->context, ZMQ_REP);
//            //                this->item.socket = *this->socket;
//            //                this->item.events = ZMQ_POLLIN;
//            //                std::cout << "[StreamBridge] Setting up connection on port " << options->port << "... PID: " << getpid() << "\n\n";
//            //                this->instantiate_connection = true;
//            //                this->socket->bind("tcp://*:" + options->port);
//
//            //                handle_messaging();
//            //                CommandType command = INFORMATION_TYPE;
//            //                int phase = 0;
//            //                std::string node_id = "00-00-00-00-00";
//            //                std::string data = "OK";
//            //                unsigned int nonce = 998;
//            //                Message message(node_id, command, phase, data, nonce);
//
//            //                while (1) {
//            //                    std::string req = this->recv();
//            //                    if (req != "") {
//            //                        data = "OK";
//            //                        Message eof_message(node_id, command, phase, data, nonce);
//            //                        Message msg_req(req);
//            //                        if (msg_req.get_data() == "SHUTDOWN") {
//            //                            this->shutdown();
//            //                            this->send(&eof_message);
//            //                            break;
//            //                        } else {
//            //                            this->send(&message);
//            //                        }
//            //                    }
//            //                    sleep(2);
//            //                }
//            //                break;
//            //            }
//            //            case CLIENT: {
//            //                //this->socket = new zmq::socket_t(*this->context, ZMQ_STREAM);
//            //                this->socket = new zmq::socket_t(*this->context, ZMQ_REQ);
//            //                std::cout << "[StreamBridge] Connecting..." << "\n\n";
//            //                this->socket->connect("tcp://" + options->peer_IP + ":" + options->peer_port);
//            //                this->instantiate_connection = false;
//            //                break;
//            //            }
//            //        }
//            //        return;
//            //    }
//            //    default: {
//            //        std::cout << "This parent process id is " << getpid() << " not EXITING\n";
//            //        //exit(0);
//            //        return;
//            //    }
//            //}
//        }
//        void whatami() {
//            std::cout << "I am a StreamBridge implementation." << '\n';
//        }
//
//        //Message * recv(){
//        std::string recv(){
//            //do {
//		char buffer[512];
//		memset(buffer, '\0', 512);
//
//		// CHANGE THE 14
//		size_t msg_size = zmq_recv(this->socket, buffer, 14, 0);
//		std::cout << "Received: " << buffer << "\n";
//		memset(buffer, '\0', 512);
//		msg_size = zmq_recv(this->socket, buffer, 14, 0);
//		std::cout << "Received: " << buffer << "\n";
//		memset(buffer, '\0', 512);
//		msg_size = zmq_recv(this->socket, buffer, 14, 0);
//		std::cout << "Received: " << buffer << "\n";
//
//		return buffer;
//
//                //if (zmq_poll(&this->item, 1, 100) >= 0) {
//                //    if (this->item.revents == 0) {
//                //        return "";
//                //    }
//                //    std::cout << "[StreamBridge] Receiving... PID: " << getpid() << '\n';
//                //    zmq::message_t request;
//                //    this->socket->recv(&request);
//
//                //    std::string req = std::string(static_cast<char *>(request.data()), request.size());
//
//                //    std::cout << "[StreamBridge] Received: " << req << "\n";
//
//                //    //Message recv_message(req);
//                //    //std::cout << "[StreamBridge] WITHIN, id: " << recv_message.get_node_id() << "\n";
//
//                //    return req;
//                //    //return &recv_message;
//                //} else {
//                //    std::cout << "Code: " << zmq_errno() << " message: " << zmq_strerror(zmq_errno()) << "\n";
//                //    exit(1);
//                //}
//            //} while ( true );
//    	}
//
//    	void send(Message * msg) {
//    	    std::cout << "[StreamBridge] Sending..." << '\n';
//	    char * response = "qwer";
//	    zmq_send(this->socket, id, id_size, ZMQ_SNDMORE);
//	    zmq_send(this->socket, response, strlen(response), ZMQ_SNDMORE);
//	    std::cout << "Sent ID followed by result.\n";
//        }
//
//    	void shutdown() {
//	    // possibly do the send 0 length message thing
//    	    //std::cout << "Shutting down socket and context\n";
//    	    //zmq_close(this->socket);
//    	    //zmq_ctx_destroy(this->context);
//    	}
//
//    	//void handle_messaging() {
//        //    CommandType command = INFORMATION_TYPE;
//        //    int phase = 0;
//        //    std::string node_id = "00-00-00-00-00";
//        //    std::string data = "OK";
//        //    unsigned int nonce = 998;
//        //    Message message(node_id, command, phase, data, nonce);
//
//        //    while (1) {
//        //        std::string req = this->recv();
//        //        if (req != "") {
//        //            data = "OK";
//        //            Message eof_message(node_id, command, phase, data, nonce);
//        //            Message msg_req(req);
//        //            if (msg_req.get_data() == "SHUTDOWN") {
//        //                this->shutdown();
//        //                this->send(&eof_message);
//        //                exit(0);
//        //                //break;
//        //            } else {
//        //                this->send(&message);
//        //            }
//        //        }
//        //        sleep(2);
//        //    }
//    	//}
//};


class TCP : public Connection {
    private:
        bool control;

        std::string port;
        std::string peer_IP;
        std::string peer_port;

	int server_fd, new_socket, valread;
	struct sockaddr_in address;
	int opt = 1;
	int addrlen = sizeof(address);
	char buffer[1024] = {0};
	char * hello = "asdf";

    public:
        TCP() {}
        TCP(Options *options) {
            std::cout << "[TCP] Creating TCP instance.\n";

            this->port = options->port;
            this->peer_IP = options->peer_IP;
            this->peer_port = options->peer_port;
            this->control = options->is_control;

            if (options->is_control) {
                std::cout << "[TCP] Creating control socket\n";

		std::cout << "[TCP] Setting up connection on port " << options->port << "... PID: " << getpid() << "\n\n";
		setup_tcp_socket(options->port);
                return;
            }

            //std::cout << "[TCP] Creating normal socket\n";
            //this->c_pid = fork();

            //switch (this->c_pid) {
            //    case -1: {
            //        std::cout << "Error creating child process\n";
            //        return;
            //    }
            //    case 0: {
            //        sleep(1);
            //        std::cout << "[TCP] The child id is " << getpid() << "\n";
            //        this->context = new zmq::context_t(1);

            //        switch (options->operation) {
            //            case SERVER: {
            //                //this->socket = new zmq::socket_t(*this->context, ZMQ_STREAM);
            //                this->socket = new zmq::socket_t(*this->context, ZMQ_REP);
            //                this->item.socket = *this->socket;
            //                this->item.events = ZMQ_POLLIN;
            //                std::cout << "[TCP] Setting up connection on port " << options->port << "... PID: " << getpid() << "\n\n";
            //                this->instantiate_connection = true;
            //                this->socket->bind("tcp://*:" + options->port);

            //                handle_messaging();
            //                CommandType command = INFORMATION_TYPE;
            //                int phase = 0;
            //                std::string node_id = "00-00-00-00-00";
            //                std::string data = "OK";
            //                unsigned int nonce = 998;
            //                Message message(node_id, command, phase, data, nonce);

            //                while (1) {
            //                    std::string req = this->recv();
            //                    if (req != "") {
            //                        data = "OK";
            //                        Message eof_message(node_id, command, phase, data, nonce);
            //                        Message msg_req(req);
            //                        if (msg_req.get_data() == "SHUTDOWN") {
            //                            this->shutdown();
            //                            this->send(&eof_message);
            //                            break;
            //                        } else {
            //                            this->send(&message);
            //                        }
            //                    }
            //                    sleep(2);
            //                }
            //                break;
            //            }
            //            case CLIENT: {
            //                //this->socket = new zmq::socket_t(*this->context, ZMQ_STREAM);
            //                this->socket = new zmq::socket_t(*this->context, ZMQ_REQ);
            //                std::cout << "[TCP] Connecting..." << "\n\n";
            //                this->socket->connect("tcp://" + options->peer_IP + ":" + options->peer_port);
            //                this->instantiate_connection = false;
            //                break;
            //            }
            //        }
            //        return;
            //    }
            //    default: {
            //        std::cout << "This parent process id is " << getpid() << " not EXITING\n";
            //        //exit(0);
            //        return;
            //    }
            //}

        }

        void whatami() {
            std::cout << "I am a TCP implementation." << '\n';
        }

	void setup_tcp_socket(std::string port) {
	    // Creating socket file descriptor 
	    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) { 
		std::cout << "Socket failed\n";
		return;
	    } 

	    // Forcefully attaching socket to the port 8080 
	    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt))) { 
		std::cout << "Setsockopt failed\n";
		return;
	    } 
	    address.sin_family = AF_INET; 
	    address.sin_addr.s_addr = INADDR_ANY; 
	    int PORT = std::stoi(port);
	    address.sin_port = htons( PORT ); 

	    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address))<0) { 
		std::cout << "Bind failed\n";
		return;
	    } 
	    if (listen(server_fd, 3) < 0) { 
		std::cout << "Listen failed\n";
		return;
	    } 
	    if ((new_socket = accept(server_fd, (struct sockaddr *)&address, (socklen_t*)&addrlen))<0) { 
		std::cout << "Socket failed\n";
		return;
		perror("accept"); 
		exit(EXIT_FAILURE); 
	    } 
	    //valread = read( new_socket , buffer, 1024); 
	    //printf("%s\n",buffer ); 
	    //send(new_socket , hello , strlen(hello) , 0 ); 
	    //printf("Hello message sent\n"); 
        }

        ////Message * recv(){
        //std::string recv(){
        //    //do {
	//	char buffer[512];
	//	memset(buffer, '\0', 512);

	//	// CHANGE THE 14
	//	size_t msg_size = zmq_recv(this->socket, buffer, 14, 0);
	//	std::cout << "Received: " << buffer << "\n";
	//	memset(buffer, '\0', 512);
	//	msg_size = zmq_recv(this->socket, buffer, 14, 0);
	//	std::cout << "Received: " << buffer << "\n";
	//	memset(buffer, '\0', 512);
	//	msg_size = zmq_recv(this->socket, buffer, 14, 0);
	//	std::cout << "Received: " << buffer << "\n";

	//	return buffer;

        //        //if (zmq_poll(&this->item, 1, 100) >= 0) {
        //        //    if (this->item.revents == 0) {
        //        //        return "";
        //        //    }
        //        //    std::cout << "[TCP] Receiving... PID: " << getpid() << '\n';
        //        //    zmq::message_t request;
        //        //    this->socket->recv(&request);

        //        //    std::string req = std::string(static_cast<char *>(request.data()), request.size());

        //        //    std::cout << "[TCP] Received: " << req << "\n";

        //        //    //Message recv_message(req);
        //        //    //std::cout << "[TCP] WITHIN, id: " << recv_message.get_node_id() << "\n";

        //        //    return req;
        //        //    //return &recv_message;
        //        //} else {
        //        //    std::cout << "Code: " << zmq_errno() << " message: " << zmq_strerror(zmq_errno()) << "\n";
        //        //    exit(1);
        //        //}
        //    //} while ( true );
    	//}

    	//void send(Message * msg) {
    	//    std::cout << "[TCP] Sending..." << '\n';
	//    char * response = "qwer";
	//    zmq_send(this->socket, id, id_size, ZMQ_SNDMORE);
	//    zmq_send(this->socket, response, strlen(response), ZMQ_SNDMORE);
	//    std::cout << "Sent ID followed by result.\n";

    	//    //std::cout << "[TCP] Sending..." << '\n';
    	//    //std::string msg_pack = msg->get_pack();
    	//    //zmq::message_t request(msg_pack.size());
    	//    //memcpy(request.data(), msg_pack.c_str(), msg_pack.size());
    	//    //this->socket->send(request);
    	//    //std::cout << "[TCP] Sent: " << msg_pack << '\n';
        //}

    	//void shutdown() {
	//    // possibly do the send 0 length message thing
    	//    //std::cout << "Shutting down socket and context\n";
    	//    //zmq_close(this->socket);
    	//    //zmq_ctx_destroy(this->context);
    	//}

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
