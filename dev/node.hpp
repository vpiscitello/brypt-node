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
#include "connection.hpp"

class Node {

    private:
        // Private Variables
        // Identification Variables
        unsigned long id;                                          // Network identification number of the node
        unsigned long serial;                                      // Hardset identification number of the device
        std::vector<std::string> communicationTechnologies;        // Communication technologies of the node

        // Adressing Variables
        std::string ip;                                            // IP address of the node
        unsigned int port;                                         // Networking port of the node

        // Cluster Variables
        unsigned long cluster;                                     // Cluster identification number of the node's cluster
        unsigned long coordinator;                                 // Coordinator identification number of the node's coordinator
        std::vector<std::string> nieghborTable;                    // Neighbor table
        unsigned int neighborCount;                                // Number of neighbors to the node

        // Network Variables
        std::string authorityAddress;                              // Networking address of the central authority for the Brypt ecosystem
        std::string networkToken;                                  // Access token for the Brypt network
        unsigned int knownNodes;                                   // The number of nodes the node has been in contact with
        std::vector<Connection *> connections;                     // A vector of open connections
		Connection * init_conn;

        // Node Type Variables
        bool isRoot;                                               // A boolean value of the node's root status
        bool isBranch;                                             // A boolean value of the node's coordinator status
        bool isLeaf;                                               // A boolean value of the node's leaf status

        // Sensor Variables
        unsigned short readingPin;                                 // The GPIO pin the node will read from

        // Lifecycle Variables
        std::time_t uptime;                                        // The amount of time the node has been live
        std::time_t addTimestamp;                                  // The timestamp the node was added to the network
        std::time_t updateTimestamp;                               // The timestamp the node was last updated

        // Security Scheme Variables
        std::string protocol;

        // Private Functions
        // Utility Functions
        void pack();                                              // Pack some data into a string
        void unpack();                                            // Unpack some data into a type
        long long getCurrentTimestamp();                        // Get the current epoch timestamp

        // Communication Functions
        void listen();                                           // Open a socket to listening for network commands
        bool contactAuthority();                                // Contact the central authority for some service
        bool notifyAddressChange();                             // Notify the cluster of some address change
        int determineConnectionMethod();                       // Determine the connection method for a particular transmission

        // Election Functions
        bool vote();                                              // Vote for leader of the cluster
        bool election();                                         // Call for an election for cluster leader
        float determineNodePower();                             // Determine the node value to the network
        bool transform();                                        // Transform the node's function in the cluster/network


    public:
        // Public Functions
        // Constructors and Deconstructors
        Node();
        ~Node();

        // Setup Functions
        void setup(Options options);

        // Information Functions
        // std::map<std::string, std::string> getDeviceInformation();                       // Get the compiled device information as a map

        // Connection Functions
        void connect();
        std::string get_local_address();

        // Communication Functions
        void setup();                                            // Setup the node
        bool transmit();                                        // Transmit some message
        bool receive();                                         // Recieve some message

};

#endif
