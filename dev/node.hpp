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
#include "control.hpp"
#include "notifier.hpp"
#include "connection.hpp"
#include "message.hpp"
#include "mqueue.hpp"
#include "command.hpp"

class Node {

    private:
        // Private Variables
        // Identification Variables
        std::string id;                                            // Network identification number of the node
        std::string serial;                                        // Hardset identification number of the device
        std::vector<TechnologyType> communication_technologies;     // Communication technologies of the node

        // Adressing Variables
        std::string addr;                                          // IP address of the node
        unsigned int port;                                         // Networking port of the node
        unsigned int next_full_port;

        // Cluster Variables
        unsigned long cluster;                                     // Cluster identification number of the node's cluster
        std::string coordinator_id;                                // Coordinator identification number of the node's coordinator
        std::string coordinator_addr;
        std::string coordinator_port;

        // Network Variables
        std::string authorityAddress;                              // Networking address of the central authority for the Brypt ecosystem
        std::string networkToken;                                  // Access token for the Brypt network
        unsigned int knownNodes;                                   // The number of nodes the node has been in contact with

        // Connection Variables
        Control * control;
        Notifier * notifier;
        // Change to hash table? based on peer name?
        std::vector<Connection *> connections;                     // A vector of open connections

        // Node Type Variables
        DeviceOperation operation;                                 // A boolean value of the node's root status
        TechnologyType coordinator_technology;

        // Sensor Variables
        unsigned short readingPin;                                 // The GPIO pin the node will read from

        // Lifecycle Variables
        std::time_t uptime;                                        // The amount of time the node has been live
        std::time_t addTimestamp;                                  // The timestamp the node was added to the network
        std::time_t updateTimestamp;                               // The timestamp the node was last updated

        // Security Scheme Variables
        std::string protocol;

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
