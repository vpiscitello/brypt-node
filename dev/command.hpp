#ifndef COMMAND_HPP
#define COMMAND_HPP

#include "utility.hpp"
#include "node.hpp"
#include "state.hpp"
#include "message.hpp"
#include "notifier.hpp"

std::string generate_reading();
std::string generate_node_info(class Node * node, struct State * state);

class Command {
    protected:
        class Node * node_instance;
        State * state;

        int phase;
        Message * message;

        bool aggregation_needed = false;
        unsigned int nodes_expected = 0;
        unsigned int nodes_responded = 0;
        std::string response;

    public:
        // Method method;
        virtual void whatami() = 0;
        virtual Message handle_message(class Message * message) = 0;
        void unspecial() {
            std::cout << "I am calling an unspecialized function." << '\n';
        }
};

// Handle Requests regarding Node information
class Information : public Command {
    private:
        enum Phase { FLOOD_PHASE, RESPOND_PHASE, CLOSE_PHASE };
    public:
        Information(class Node * instance, struct State * state) {
            this->node_instance = instance;
            this->state = state;
        }
        void whatami() {
            printo("Handling response to Information request", COMMAND_P);
        }

        Message handle_message(class Message * message);

        void flood_handler(Self * self, Message * message, class Notifier * notifier);
        void respond_handler();
        void close_handler();
};

// Handle Requests regarding Sensor readings
class Query : public Command {
    private:
        enum Phase { FLOOD_PHASE, RESPOND_PHASE, AGGREGATE_PHASE, CLOSE_PHASE };
    public:
        Query(class Node * instance, struct State * state) {
            this->node_instance = instance;
            this->state = state;
        }
        void whatami() {
            printo("Handling response to Query request", COMMAND_P);
        }

        Message handle_message(class Message * message);

        void flood_handler(Self * self, Network * network, Message * message, class Notifier * notifier) ;
        void respond_handler(Self * self, Message * message, Connection * connection);
        void aggregate_handler(Self * self, Message * message, MessageQueue * message_queue);
        void close_handler();
};

// Handle Requests regarding Elections
class Election : public Command {
    private:
        enum Phase { PROBE_PHASE, PRECOMMIT_PHASE, VOTE_PHASE, ABORT_PHASE, RESULTS_PHASE, CLOSE_PHASE };
    public:
        Election(class Node * instance, struct State * state) {
            this->node_instance = instance;
            this->state = state;
        }
        void whatami() {
            printo("Handling response to Election request", COMMAND_P);
        }

        Message handle_message(class Message * message);

        void probe_handler();
        void precommit_handler();
        void vote_handler();
        void abort_handler();
        void results_handler();
        void close_handler();
};

// Handle Requests regarding Transforming Node type
class Transform : public Command {
    private:
        enum Phase { INFO_PHASE, HOST_PHASE, CONNECT_PHASE, CLOSE_PHASE };
    public:
        Transform(class Node * instance, struct State * state) {
            this->node_instance = instance;
            this->state = state;
        }
        void whatami() {
            printo("Handling response to Transform request", COMMAND_P);
        }

        Message handle_message(class Message * message);

        void info_handler();
        void host_handler();
        void connect_handler();
        void close_handler();
};

// Handle Requests regarding Connecting to a new network or peer
class Connect : public Command {
    private:
        enum Phase { CONTACT_PHASE, JOIN_PHASE, CLOSE_PHASE };
    public:
        Connect(class Node * instance, struct State * state) {
            this->node_instance = instance;
            this->state = state;
        }
        void whatami() {
            printo("Handling response to Connect request", COMMAND_P);
        }

        Message handle_message(class Message * message);

        void contact_handler();
        void join_handler(Self * self, Network * network, Message * message, Control * control);
        void close_handler();
};



inline Command* CommandFactory(CommandType command, class Node * instance, struct State * state) {
    switch (command) {
        case INFORMATION_TYPE:
            return new Information(instance, state);
            break;
        case QUERY_TYPE:
            return new Query(instance, state);
            break;
        case ELECTION_TYPE:
            return new Election(instance, state);
            break;
        case TRANSFORM_TYPE:
            return new Transform(instance, state);
            break;
        case CONNECT_TYPE:
            return new Connect(instance, state);
            break;
        case NO_CMD:
            return NULL;
            break;
    }
}

#endif
