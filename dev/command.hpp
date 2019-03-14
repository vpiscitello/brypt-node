#ifndef COMMAND_HPP
#define COMMAND_HPP

#include "utility.hpp"
#include "message.hpp"

class Command {
    protected:
        int phase;
        Message * message;

        bool aggregation_needed = false;
        unsigned int nodes_expected = 0;
        unsigned int nodes_responded = 0;
        std::string response;

    public:
        // Method method;
        virtual void whatami() = 0;
        virtual Message handle_message(Message * message) = 0;
        void unspecial() {
            std::cout << "I am calling an unspecialized function." << '\n';
        }
};

// Handle Requests regarding Node information
class Information : public Command {
    private:
        enum Phase { PRIVATE_PHASE, NETWORK_PHASE, CLOSE_PHASE };
    public:
        void whatami() {
            std::cout << "== [Command] Handling response to Information request" << '\n';
        }
        Message handle_message(Message * message) {
            Message response;

            this->whatami();
            std::cout << "Handling Message..." << '\n';
            this->message = message;
            if ( message->get_phase() == 0 ) {
                /* code */
            }

            return response;
        }
};

// Handle Requests regarding Sensor readings
class Query : public Command {
    private:
        enum Phase { PASSDOWN_PHASE, SENSOR_PHASE, CLOSE_PHASE };
    public:
        void whatami() {
            std::cout << "== [Command] Handling response to Query request" << '\n';
        }
        Message handle_message(Message * message) {
            Message response;

            this->whatami();
            std::cout << "Handling Message..." << '\n';
            this->message = message;
            if ( message->get_phase() == 0 ) {
                /* code */
            }

            return response;
        }
};

// Handle Requests regarding Elections
class Election : public Command {
    private:
        enum Phase { PROBE_PHASE, PRECOMMIT_PHASE, VOTE_PHASE, ABORT_PHASE, RESULTS_PHASE, CLOSE_PHASE };
    public:
        void whatami() {
            std::cout << "== [Command] Handling response to Election request" << '\n';
        }
        Message handle_message(Message * message) {
            Message response;

            this->whatami();
            std::cout << "Handling Message..." << '\n';
            this->message = message;
            if ( message->get_phase() == 0 ) {
                /* code */
            }

            return response;
        }
};

// Handle Requests regarding Transforming Node type
class Transform : public Command {
    private:
        enum Phase { INFO_PHASE, HOST_PHASE, CONNECT_PHASE, CLOSE_PHASE };
    public:
        void whatami() {
            std::cout << "== [Command] Handling response to Transform request" << '\n';
        }
        Message handle_message(Message * message) {
            Message response;

            this->whatami();
            std::cout << "Handling Message..." << '\n';
            this->message = message;
            if ( message->get_phase() == 0 ) {
                /* code */
            }

            return response;
        }
};

// Handle Requests regarding Connecting to a new network or peer
class Connect : public Command {
    private:
        enum Phase { CONTACT_PHASE, JOIN_PHASE, CLOSE_PHASE };
    public:
        void whatami() {
            std::cout << "== [Command] Handling response to Connect request" << '\n';
        }
        Message handle_message(Message * message) {
            Message response;

            this->whatami();
            std::cout << "Handling Message..." << '\n';
            this->message = message;

            this->phase = this->message->get_phase();

            switch ( ( Phase )this->phase ) {
                case CONTACT_PHASE:
                    break;
                case JOIN_PHASE:
                    break;
                case CLOSE_PHASE:
                    break;
            }

            return response;
        }
};



inline Command* CommandFactory(CommandType command) {
    switch (command) {
        case INFORMATION_TYPE:
            return new Information;
            break;
        case QUERY_TYPE:
            return new Query;
            break;
        case ELECTION_TYPE:
            return new Election;
            break;
        case TRANSFORM_TYPE:
            return new Transform;
            break;
        case CONNECT_TYPE:
            return new Connect;
            break;
        case NO_CMD:
            return NULL;
            break;
    }
}


#endif
