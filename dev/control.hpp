#ifndef CONTROL_HPP
#define CONTROL_HPP

#include "connection.hpp"
#include "utility.hpp"

#include <sys/socket.h>

class Control {
    private:
        Self * self;
        Connection * conn;

    public:
        //Control() {}
        Control(TechnologyType technology, Self * self) {
            this->self = self;

            Options control_setup;

            control_setup.technology = technology;
            control_setup.port = (*this->self).port;
            control_setup.operation = ROOT;
            control_setup.is_control = true;

            // this->node_id = *self.node_id;
            // this->next_full_port = *self.next_full_port;

            this->conn = ConnectionFactory(technology, &control_setup);
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
            std::string request = this->conn->recv(ZMQ_NOBLOCK);
            //std::string request = this->conn->recv(0);//blocking mode

            switch (request.size()) {
                case 0: {
                    return "";
                }
                case 1: {
                    printo("Recieved connection byte", CONTROL_P);

                    if (request == "\x06") {
                        printo("Device connection acknowledgement", CONTROL_P);

                        this->conn->send("\x06");
                        printo("Device was sent acknowledgement", CONTROL_P);

                        request = this->conn->recv(0);

                        std::cout << "Request was " << request << "\n";

			int comm_requested = (int)request[0] - 48;
			if (comm_requested >= 0 && comm_requested <= 6) {
			    printo("Communication type requested: " + std::to_string(comm_requested), CONTROL_P);
			    TechnologyType server_comm_type;
			    if ((TechnologyType)comm_requested == TCP_TYPE) {
                    server_comm_type = STREAMBRIDGE_TYPE;
			    } else {
                    server_comm_type = (TechnologyType)comm_requested;
			    }
			    std::string device_info = this->handle_contact(server_comm_type);

			    return device_info;
			} else {
			    printo("Somethings not right", CONTROL_P);
			    try {
                    this->conn->send("\x15");
			    } catch(...) {

			    }
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

        /* **************************************************************************
        ** Function:
        ** Description:
        ** *************************************************************************/
        std::string handle_contact(TechnologyType technology) {
            printo("Handling request from control socket", CONTROL_P);

            switch (technology) {
        		case TCP_TYPE:
        		case STREAMBRIDGE_TYPE:
                case DIRECT_TYPE: {
                    std::string full_port = "";
                    std::string device_info = "";

                    (*this->self).next_full_port++;
                    full_port = std::to_string((*this->self).next_full_port);

                    printo("Sending port: " + full_port, CONTROL_P);
                    Message port_message((*this->self).id, "We'll Cross that Brypt When We Come to It.", CONNECT_TYPE, 0, full_port, 0);
                    this->conn->send(&port_message);

                    device_info = this->conn->recv(0);
                    printo("Received: " + device_info, CONTROL_P);

                    return device_info;
                }
                default: {
                    this->conn->send("\x15");
                }
            }

            return "";

        }

	void close_current_connection() {
	    if (this->conn->get_internal_type() == "TCP") {
            this->conn->prepare_for_next();
	    }
	}

};

#endif
