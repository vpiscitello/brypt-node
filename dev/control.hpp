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

	void listen() {
	    this->conn->serve(0);
	};
};

#endif
