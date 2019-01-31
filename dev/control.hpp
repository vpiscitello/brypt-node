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
	    this->conn = ConnectionFactory(t, &control_setup);
	}

	void restart() {
	
	};

	//possibly create an optional parameter for serve so that this can override and get all output
	std::string listen() {
	    std::string req = this->conn->serve();
	    //std::cout << "Received request: " << req << "\n";
	    if (req == "HELLO") {
		this->conn->send("HELLO");
		req = this->conn->serve();
		//std::cout << "Received request2: " << req << "\n";

		if (req.compare(0, 7, "CONNECT") == 0) {
		    std::cout << "Was sent CONNECT\n";
		    std::string params = req.substr(8, req.length());
		    std::cout << "The remaining string is: " << params << "\n";
		    return "WIFI";
		}
	    } else if (req == "EOF") {
		this->conn->send("EOF");
		std::cout << "Client sent EOF, sending back EOF\n\n";
		return "";
	    }
	    return "";
	};

	void send(std::string message) {
	    this->conn->send(message);
	}
};

#endif
