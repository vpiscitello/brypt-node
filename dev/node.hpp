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
#include "await.hpp"
#include "watcher.hpp"

class Node {

    private:
        // Private Variables
        // State struct
        State state;

        // Connection Variables
        class Control * control;
        class Notifier * notifier;
        class PeerWatcher * watcher;
        // Change to hash table? based on peer name?
        std::vector<class Connection *> connections;                     // A vector of open connections

        // Node Commands
        // Change to hash table? based on command name?
        std::vector<class Command *> commands;                           // A vector of possible commands to be handled

        // Message Queue
        class MessageQueue message_queue;
        class AwaitContainer awaiting;

        // Private Functions
        // Utility Functions
        float determine_node_power();                           // Determine the node value to the network
        int determine_connection_method();                       // Determine the connection method for a particular transmission
        TechnologyType determine_best_connection_type();          // Determine the best connection type the node has
        void add_connection(Connection *);

        // Communication Functions
        void initial_contact(Options *opts);
        void join_coordinator();
        bool contact_authority();                                // Contact the central authority for some service
        bool notify_address_change();                           // Notify the cluster of some address change

        // Request Handlers
        void handle_control_request(std::string message);
        void handle_notification(std::string message);
        void handle_queue_request(Message * message);
        void handle_fulfilled();

        // Election Functions
        bool election();                                         // Call for an election for cluster leader
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
        Connection * setup_wifi_connection(std::string peer_id, std::string port);

        // Getter Functions
        class Control * get_control();
        class Notifier * get_notifier();
        std::vector<class Connection *> * get_connections();
        class Connection * get_connection(unsigned int index);
        class MessageQueue * get_message_queue();
        class AwaitContainer * get_awaiting();

        // Communication Functions
        void notify_connection(std::string id);

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
