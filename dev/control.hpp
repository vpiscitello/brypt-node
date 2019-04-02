#ifndef CONTROL_HPP
#define CONTROL_HPP

#include "connection.hpp"
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

            switch (request.size()) {
                case 0: {
                    return "";
                }
                case 1: {
                    std::cout << "== [Control] Recieved connection byte\n";

                    if (request == "\x06") {
                        std::cout << "== [Control] Device connection acknowledgement\n";

                        this->conn->send("\x06");
                        std::cout << "== [Control] Device was sent acknowledgement\n";

                        request = this->conn->recv();

                        if (request.compare(0, 1, "\x16") == 0) {
                            std::cout << "== [Control] Device waiting for connection port\n";
                            // TODO: Add connection type byte

                            std::string device_info = this->handle_contact(DIRECT_TYPE);

                            return device_info;
                        } else {
                            std::cout << "\n== [Control] Somethings not right" << '\n';
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
            std::cout << "== [Node] Handling request from control socket\n";

            switch (technology) {
                case DIRECT_TYPE: {
                    std::string full_port = "";
                    std::string device_info = "";

                    (*this->self).next_full_port++;
                    full_port = std::to_string((*this->self).next_full_port);

                    std::cout << "== [Control] Sending port: " << full_port << "\n";
                    Message port_message((*this->self).id, "We'll Cross that Brypt When We Come to It.", CONNECT_TYPE, 0, full_port, 0);
                    this->conn->send(&port_message);

                    device_info = this->conn->recv();
                    std::cout << "== [Control] Received: " << device_info << "\n";

                    return device_info;
                }
                default: {
                    this->conn->send("\x15");
                }
            }

            return "";

        }

};

#endif
