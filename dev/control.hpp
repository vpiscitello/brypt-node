#ifndef CONTROL_HPP
#define CONTROL_HPP

#include "connection.hpp"

class Control {
    private:
	Connection * conn;

    public:
	//Control() {}
	Control(TechnologyType t, std::string port) {
	    Options control_setup;

	    control_setup.technology = t;
	    control_setup.port = port;
	    control_setup.operation = SERVER;
	    control_setup.is_control = true;

	    this->conn = ConnectionFactory(t, &control_setup);
	}

	void restart() {

	};

	// Passthrough for send function of the connection type
	void send(Message * message) {
	    this->conn->send(message);
	}

    // Passthrough for send function of the connection type
    void send(const char * message) {
        this->conn->send(message);
    }

    // Receive for requests, if a request is received recv it and then return the message string
    std::string recv() {
        std::string request = this->conn->recv();

        switch (request.size()) {
            case 0: {
                return "";
            }
            case 1: {
                std::cout << "== [CONTROL] Recieved connection byte\n";

                if (request == "\x06") {
                    std::cout << "== [CONTROL] Device connection acknowledgement\n";

                    this->conn->send("\x06");
                    std::cout << "== [CONTROL] Device was sent acknowledgement\n";

                    request = this->conn->recv();

                    if (request.compare(0, 1, "\x16") == 0) {
                        std::cout << "== [CONTROL] Device waiting for connection port\n";
                        return "WIFI";
                    }

                }
            }
            default: {
                Message message(request);
                return message.get_pack();
            }
        }

        return "";
    };

};

#endif
