#ifndef CONTROL_HPP
#define CONTROL_HPP

#include "connection.hpp"

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
	}

	void restart() {
	
	};

	// Listen for requests, if a request is received serve it and then return the request back to node.cpp listen function
	std::string listen() {
	    std::string req = this->conn->serve();
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
			req = this->conn->serve();
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
	    std::string req = this->conn->serve();
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
