#ifndef CONNECTION_HPP
#define CONNECTION_HPP

#include <unistd.h>
#include <vector>

#include "zmq.hpp"

#include "utility.hpp"
#include "message.hpp"

// Includes to be moved to commands
#include "json11.hpp" // JSON handling
#include <time.h> // Random reading
#include <chrono> // Timestamp


class Connection {
    protected:
        bool active;
        bool instantiate_connection;
	
	// each connection should have two named pipes
	bool data_available;
	
	pid_t c_pid;

    public:
        // Method method;
        virtual void whatami() = 0;
        //virtual Message * serve() = 0;
        virtual std::string serve() = 0;
        virtual void send(Message *) = 0;
        virtual void shutdown() = 0;

        void pass_up_message() {

        }

        void unspecial() {
            std::cout << "I am calling an unspecialized function." << '\n';
        }
};

class Direct : public Connection {
    private:
        std::string port;
        std::string peer_IP;
        std::string peer_port;
	bool control;


        zmq::context_t *context;
        zmq::socket_t *socket;
        zmq::pollitem_t item;
  
        void serve(){
          srand(time(NULL));
            do {
                std::cout << "Receiving..." << '\n';
                zmq::message_t request;
                this->socket->recv( &request );
                //std::cout << std::string( static_cast< char * >( request.data() ), request.size() ) << "\n\n";


                std::string req_raw( static_cast< char * >( request.data() ), request.size() );
                class Message req_message( req_raw );
                std::cout << req_raw << "\n" << req_message.get_data() << "\n\n";

                sleep( 2 );

                std::string node_id = "1";
                CommandType command = req_message.get_command();
                int phase = req_message.get_phase() + 1;
                std::string data;
                unsigned int nonce = req_message.get_nonce() + 1;

                switch(req_message.get_command()) {

                  case INFORMATION_TYPE: {

                    // Move to utility header (will be used in message class and command class
                    std::stringstream epoch_ss;
                    std::chrono::seconds seconds;
                    std::chrono::time_point<std::chrono::system_clock> current_time;
                    current_time = std::chrono::system_clock::now();
                    seconds = std::chrono::duration_cast<std::chrono::seconds>( current_time.time_since_epoch() );
                    epoch_ss.clear();
                    epoch_ss << seconds.count();
                    std::string epoch_str = epoch_ss.str();


                    json11::Json nodes_json = json11::Json::array {
                      json11::Json::object { 
                        { "uid", "1" },
                        { "cluster", 0 },
                        { "coordinator", 1 },
                        { "neighbor_count", 4 },
                        { "designation", "root" },
                        { "comm_techs", json11::Json::array { "WiFi" } },
                        { "update_timestamp", epoch_str }
                      },
                      json11::Json::object { 
                        { "uid", "2" },
                        { "cluster", 0 },
                        { "coordinator", 1 },
                        { "neighbor_count", 4 },
                        { "designation", "node" },
                        { "comm_techs", json11::Json::array { "WiFi" } },
                        { "update_timestamp", epoch_str }
                      },
                      json11::Json::object { 
                        { "uid", "3" },
                        { "cluster", 0 },
                        { "coordinator", 1 },
                        { "neighbor_count", 6 },
                        { "designation", "coordinator" },
                        { "comm_techs", json11::Json::array { "WiFi", "LoRa" } },
                        { "update_timestamp", epoch_str }
                      },
                      json11::Json::object { 
                        { "uid", "6" },
                        { "cluster", 0 },
                        { "coordinator", 0 },
                        { "neighbor_count", 4 },
                        { "designation", "node" },
                        { "comm_techs", json11::Json::array { "WiFi" } },
                        { "update_timestamp", epoch_str }
                      }
                    };

                    data = nodes_json.dump();

                    break;
                  }

                  case QUERY_TYPE: {	

                    json11::Json::object msg_obj;

                    for(int idx = 1; idx <= 6; idx++) {
                      int reading = rand() % ( 74 - 68 ) + 68;

                      // Move to utility header (will be used in message class and command class
                      std::stringstream epoch_ss;
                      std::chrono::seconds seconds;
                      std::chrono::time_point<std::chrono::system_clock> current_time;
                      current_time = std::chrono::system_clock::now();
                      seconds = std::chrono::duration_cast<std::chrono::seconds>( current_time.time_since_epoch() );
                      epoch_ss.clear();
                      epoch_ss << seconds.count();
                      std::string epoch_str = epoch_ss.str();

                      json11::Json reading_json = json11::Json::object({
                        { "reading", reading },
                        { "timestamp", epoch_str }
                      });

                      std::string r_data = reading_json.dump();

                      class Message msg( std::to_string(idx), command, phase, r_data, nonce );

                      std::string msg_pack = msg.get_pack();

                      if(idx == 6) {
                        int include_con = rand() % 100;
                        if(include_con < 23) {
                          msg_obj[std::to_string(idx)] = "";
                          break;
                        }
                      }


                      msg_obj[std::to_string(idx)] = msg_pack;

                    }


                    json11::Json msg_json = json11::Json::object(msg_obj);
                    data.append(msg_json.dump());

                    break;
                  }
                  default: {
                    break;
                  }

			}
			
			// Send message up to Node to handle the command Request
			// Wait for command response from Node
			// Send command response	

			class Message rep_message( node_id, command, phase, data, nonce );

			std::string response = rep_message.get_pack();

			zmq::message_t reply( response.size() );
        		memcpy( reply.data(), response.c_str(), response.size() );
        		this->socket->send( reply );
        	} while ( true );
        }

        void send(){
            do {
		    std::cout << "Sending..." << '\n';
		    //std::string message = "Request.";

		    std::string node_id = "1";
	    	    CommandType command = QUERY_TYPE;
		    int phase = 0;
		    std::string data = "Request.";
		    unsigned int nonce = 0;
		    
		    class Message req_message( node_id, command, phase, data, nonce );
		    std::string req = req_message.get_pack();
	
		    zmq::message_t request( req.size() );
		    memcpy( request.data(), req.c_str(), req.size() );
		    this->socket->send( request );

		    // Send message up to Node to handle the command Request
		    // Wait for command response from Node
		    // Send command response

		    zmq::message_t reply;
		    this->socket->recv( &reply );
		    
		    //std::cout << std::string( static_cast< char * >( reply.data() ), reply.size() ) << "\n\n";
		    std::string rep_raw( static_cast< char * >( reply.data() ), reply.size() );
		    class Message rep_message( rep_raw );
		    std::cout << rep_raw << "\n" << rep_message.get_data() << "\n\n";
		    sleep( 5 );
	    } while ( true );
	}

    public:
        Direct() {}
        Direct(Options *options) {
        std::cout << "Creating direct instance.\n";
        this->port = options->port;
        this->peer_IP = options->peer_IP;
        this->peer_port = options->peer_port;
        this->control = options->is_control;

	    if (options->is_control) {
        std::cout << "[Direct] Creating control socket\n";

        this->context = new zmq::context_t(1);

        switch (options->operation) {
            case SERVER: {
              //this->socket = new zmq::socket_t(*this->context, ZMQ_STREAM);
              this->socket = new zmq::socket_t(*this->context, ZMQ_REP);
              this->item.socket = *this->socket;
              this->item.events = ZMQ_POLLIN;
              std::cout << "[Control] Setting up connection on port " << options->port << "... PID: " << getpid() << "\n\n";
              this->instantiate_connection = true;
              this->socket->bind("tcp://*:" + options->port); 
              break;
            }
            case CLIENT: {
              //this->socket = new zmq::socket_t(*this->context, ZMQ_STREAM);
              this->socket = new zmq::socket_t(*this->context, ZMQ_REQ);
              this->item.socket = *this->socket;
              this->item.events = ZMQ_POLLIN;
              std::cout << "[Direct Control Client] Connecting to: " << options->peer_IP << ":" << options->peer_port << "\n\n";
              this->socket->connect("tcp://" + options->peer_IP + ":" + options->peer_port);
              this->instantiate_connection = false;
              break;
            }
	    	}
		    return;
	    }
	    std::cout << "[Direct] Creating normal socket\n";
	    this->c_pid = fork();
	    switch (this->c_pid) {
		    case -1:
          std::cout << "Error creating child process\n";
          return;
        case 0:
            sleep(1);
            std::cout << "[Direct] The child id is " << getpid() << "\n";
            this->context = new zmq::context_t(1);
          
            switch (options->operation) {
              case SERVER: {
                //this->socket = new zmq::socket_t(*this->context, ZMQ_STREAM);
                this->socket = new zmq::socket_t(*this->context, ZMQ_REP);
                this->item.socket = *this->socket;
                this->item.events = ZMQ_POLLIN;
                std::cout << "[Direct] Setting up connection on port " << options->port << "... PID: " << getpid() << "\n\n";
                this->instantiate_connection = true;
                this->socket->bind("tcp://*:" + options->port);

                handle_messaging();
                CommandType command = INFORMATION_TYPE;
                int phase = 0;
                std::string node_id = "00-00-00-00-00";
                std::string data = "OK";
                unsigned int nonce = 998;
                Message message(node_id, command, phase, data, nonce);

                while (1) {
                    std::string req = this->serve();
                    if (req != "") {
                        data = "OK";
                        Message eof_message(node_id, command, phase, data, nonce);
                        Message msg_req(req);
                        if (msg_req.get_data() == "SHUTDOWN") {
                          this->shutdown();
                          this->send(&eof_message);
                          break;
                        } else {
                          this->send(&message);
                        }
                    }
                    sleep(2);
                  }
                  break;
              }
              case CLIENT: {
                //this->socket = new zmq::socket_t(*this->context, ZMQ_STREAM);
                this->socket = new zmq::socket_t(*this->context, ZMQ_REQ);
                std::cout << "[Direct] Connecting..." << "\n\n";
                this->socket->connect("tcp://" + options->peer_IP + ":" + options->peer_port);
                this->instantiate_connection = false;
                break;
              }
            }
            return;
            default:
                std::cout << "This parent process id is " << getpid() << " not EXITING\n";
                //exit(0);
                return;
              }
        }
        void whatami() {
            std::cout << "I am a Direct implementation." << '\n';
        }

    //Message * serve(){
    std::string serve(){
        do {
      if (zmq_poll(&this->item, 1, 100) >= 0) {
          if (this->item.revents == 0) {
        return "";
          }
          std::cout << "[Direct] Receiving... PID: " << getpid() << '\n';
          zmq::message_t request;
          this->socket->recv(&request);

          std::string req = std::string(static_cast<char *>(request.data()), request.size());

          std::cout << "[Direct] Received: " << req << "\n";

          //Message recv_message(req);
          //std::cout << "[Direct] WITHIN, id: " << recv_message.get_node_id() << "\n";

          return req;
          //return &recv_message;
      } else {
          std::cout << "Code: " << zmq_errno() << " message: " << zmq_strerror(zmq_errno()) << "\n";
          exit(1);
      }
	    } while ( true );
	}

	void send(Message * msg) {
	    std::cout << "[Direct] Sending..." << '\n';
	    std::string msg_pack = msg->get_pack();
	    zmq::message_t request(msg_pack.size());
	    memcpy(request.data(), msg_pack.c_str(), msg_pack.size());
	    this->socket->send(request);
	    std::cout << "[Direct] Sent: " << msg_pack << '\n';
        }

	void shutdown() {
	    std::cout << "Shutting down socket and context\n";
	    zmq_close(this->socket);
	    zmq_ctx_destroy(this->context);
	}

	void handle_messaging() {
	    CommandType command = INFORMATION_TYPE;
	    int phase = 0;
	    std::string node_id = "00-00-00-00-00";
	    std::string data = "OK";
	    unsigned int nonce = 998;
	    Message message(node_id, command, phase, data, nonce);

	    while (1) {
		std::string req = this->serve();
		if (req != "") {
		    data = "OK";
		    Message eof_message(node_id, command, phase, data, nonce);
		    Message msg_req(req);
		    if (msg_req.get_data() == "SHUTDOWN") {
			this->shutdown();
			this->send(&eof_message);
			exit(0);
			//break;
		    } else {
			this->send(&message);
		    }
		}
		sleep(2);
	    }
	}
};

class Bluetooth : public Connection {
    public:
        void whatami() {
            std::cout << "I am a BLE implementation." << '\n';
        }
	
	//Message * serve(){
	std::string serve(){

	}
	
	void send(Message * msg) {

	}

	void shutdown() {

	}
};

class LoRa : public Connection {
    public:
        void whatami() {
            std::cout << "I am a LoRa implementation." << '\n';
        }

	//Message * serve(){
	std::string serve(){

	}
	
	void send(Message * msg) {

	}

	void shutdown() {

	}
};

class Websocket : public Connection {
    public:
        void whatami() {
            std::cout << "I am a Websocket implementation." << '\n';
        }

	//Message * serve(){
	std::string serve(){

	}
	
	void send(Message * msg) {

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
    }
    return NULL;
}

#endif
