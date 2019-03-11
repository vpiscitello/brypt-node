#ifndef CONTROL_HPP
#define CONTROL_HPP

#include "connection.hpp"
#include <sys/socket.h>

class Control {
    private:
       Connection * conn;

    public:
    	//Control() {}
    	Control(TechnologyType t) {
    	    Options control_setup;
    	    control_setup.technology = t;
    	    control_setup.port = "3001";
    	    control_setup.operation = SERVER;
    	    control_setup.is_control = true;
    	    this->conn = ConnectionFactory(t, &control_setup);
	    //int server_fd, new_socket, valread; 
	    //struct sockaddr_in address; 
	    //int opt = 1; 
	    //int addrlen = sizeof(address); 
	    //char buffer[1024] = {0}; 
	    //char *hello = "Hello from server"; 

	    //if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) 
	    //{ 
	    //    std::cout << "Socket failed\n"; 
	    //} 

	    //// Forcefully attaching socket to the port 8080 
	    //if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, 
	    //&opt, sizeof(opt))) 
	    //{ 
	    //    std::cout << "Setsockopt failed\n"; 
	    //} 
	    //address.sin_family = AF_INET; 
	    //address.sin_addr.s_addr = INADDR_ANY; 
	    //address.sin_port = htons( PORT ); 

	    //// Forcefully attaching socket to the port 8080 
	    //if (bind(server_fd, (struct sockaddr *)&address,  
	    //sizeof(address))<0) 
	    //{ 
	    //    std::cout << "Setsockopt failed\n"; 
	    //} 
	    //if (listen(server_fd, 3) < 0) 
	    //{ 
	    //    std::cout << "Listen failed\n"; 
	    //} 
	    //if ((new_socket = accept(server_fd, (struct sockaddr *)&address,  
	    //(socklen_t*)&addrlen))<0) 
	    //{ 
	    //    std::cout << "Accept failed\n"; 
	    //} 
	    //valread = read( new_socket , buffer, 1024); 
	    //printf("%s\n",buffer ); 
	    //send(new_socket , hello , strlen(hello) , 0 ); 
	    //printf("Hello message sent\n");
    	}

    	void restart() {

    	};

    	// Listen for requests, if a request is received recv it and then return the request back to node.cpp listen function
    	std::string listen() {
    	    std::string req = this->conn->recv();
    	    if (req != "") {
		Message msg(req);
		std::cout << "[CONTROL] Message unpacked: " << msg.get_pack() << "\n";
		if (msg.get_command() == CONNECT_TYPE) {
		    std::cout << "[CONTROL] Command is CONNECT_TYPE\n";
		    CommandType command = INFORMATION_TYPE;
		    int phase = 0;
		    std::string node_id = "00-00-00-00-00";
		    std::string data = "HELLO";
		    unsigned int nonce = 998;
		    Message message(node_id, command, phase, data, nonce);

		    this->conn->send(&message);

		    req = "";
		    while (req == "") {
			req = this->conn->recv();
		    }

		    Message msg(req);
		    std::cout << "[CONTROL] Message2 unpacked: " << msg.get_pack() << "\n";

		    std::string conn_data = msg.get_data();
		    if (conn_data.compare(0, 7, "CONNECT") == 0) {
			std::cout << "[CONTROL] Was sent CONNECT\n";
			std::string params = conn_data.substr(8, req.length());
			std::cout << "[CONTROL] The remaining string is: " << params << "\n";
			return "WIFI";
		    }
		}
    	    }
    	    return "";
    	};

    	// Listen for the EOF of a connection, if received, send back EOF
    	void eof_listen() {
    	    std::string req = this->conn->recv();
    	    if (req != "") {
		Message msg(req);
		std::cout << "[CONTROL]-EOF Listen, Message unpacked: " << msg.get_pack() << "\n";
		if (msg.get_data() == "EOF") {
		    std::cout << "[CONTROL]-EOF Listen, got EOF\n";
		    CommandType command = INFORMATION_TYPE;
		    int phase = 0;
		    std::string node_id = "00-00-00-00-00";
		    std::string data = "EOF";
		    unsigned int nonce = 998;
		    Message message(node_id, command, phase, data, nonce);

		    this->conn->send(&message);
		    std::cout << "[CONTROL]-EOF Listen, sent EOF\n";
		}
    	    }
    	};

    	// Passthrough for send function of the connection type
    	void send(Message * message) {
    	    this->conn->send(message);
    	}
};

#endif
