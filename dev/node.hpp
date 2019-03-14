#ifndef NODE_HPP
#define NODE_HPP

#include <cstdio>
#include <cstdlib>
#include <cmath>
#include <ctime>
#include <iostream>
#include <string>
#include <vector>

#include <sys/types.h>
#include <sys/socket.h>
#include <ifaddrs.h>
#include <net/if.h>
#include <netdb.h>
#include <arpa/inet.h>

#include "utility.hpp"
#include "state.hpp"
#include "control.hpp"
#include "notifier.hpp"
#include "connection.hpp"
#include "message.hpp"
#include "mqueue.hpp"
#include "command.hpp"

class Node {

    private:
        // Private Variables
        // State struct
        State state;

        // Connection Variables
        Control * control;
        Notifier * notifier;
        // Change to hash table? based on peer name?
        std::vector<Connection *> connections;                     // A vector of open connections

        // Node Commands
        std::vector<Command *> commands;                           // A vector of possible commands to be handled

        // Message Queue
        class MessageQueue message_queue;

        // Private Functions
        // Utility Functions
        int determine_connection_method();                       // Determine the connection method for a particular transmission
        TechnologyType determine_best_connection_type();          // Determine the best connection type the node has
        void add_connection(Connection *);

        // Setup Functions
        Connection * setup_wifi_connection(std::string peer_id, std::string port);

        // Communication Functions
        void initial_contact(Options *opts);
        void join_coordinator();
        bool contact_authority();                                // Contact the central authority for some service
        bool notify_address_change();                           // Notify the cluster of some address change
        void notify_connection(std::string id);

        // Request Handlers
        void handle_control_request(std::string message);
        void handle_notification(std::string message);
        void handle_queue_request(Message * message);
        //static void * connection_handler(void *);

        // Election Functions
        bool vote();                                              // Vote for leader of the cluster
        bool election();                                         // Call for an election for cluster leader
        float determine_node_power();                             // Determine the node value to the network
        bool transform();                                        // Transform the node's function in the cluster/network

        // Run Functions
        void listen();                                           // Open a socket to listening for network commands
        void connect();


    public:
        // Public Functions
        // Constructors and Deconstructors
        Node();
        ~Node();

        // Setup Functions
        void setup(Options options);
        void setup();                                            // Setup the node

        // Run Functions
        void startup();
        std::string get_local_address();

};

struct ThreadArgs {
    Node * node;
    Options * opts;
};

void * connection_handler(void *);

#endif
