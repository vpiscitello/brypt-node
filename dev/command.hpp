#ifndef COMMAND_HPP
#define COMMAND_HPP

#include "utility.hpp"
#include "message.hpp"

class Command {
    protected:
        int phase;
        Message * message;
    public:
        // Method method;
        virtual void whatami() = 0;
        virtual void handle_message(Message * message) = 0;
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
            std::cout << "I am a Information Command." << '\n';
        }
        void handle_message(Message * message) {
            this->whatami();
            std::cout << "Handling Message..." << '\n';
            this->message = message;
            if ( message->get_phase() == 0 ) {
                /* code */
            }
        }
};

// Handle Requests regarding Sensor readings
class Query : public Command {
    private:
        enum Phase { PASSDOWN_PHASE, SENSOR_PHASE, CLOSE_PHASE };
    public:
        void whatami() {
            std::cout << "I am a Query Command." << '\n';
        }
        void handle_message(Message * message) {
            this->whatami();
            std::cout << "Handling Message..." << '\n';
            this->message = message;
            if ( message->get_phase() == 0 ) {
                /* code */
            }
        }
};

// Handle Requests regarding Elections
class Election : public Command {
    private:
        enum Phase { PROBE_PHASE, PRECOMMIT_PHASE, VOTE_PHASE, ABORT_PHASE, RESULTS_PHASE, CLOSE_PHASE };
    public:
        void whatami() {
            std::cout << "I am an Election Command." << '\n';
        }
        void handle_message(Message * message) {
            this->whatami();
            std::cout << "Handling Message..." << '\n';
            this->message = message;
            if ( message->get_phase() == 0 ) {
                /* code */
            }
        }
};

// Handle Requests regarding Transforming Node type
class Transform : public Command {
    private:
        enum Phase { INFO_PHASE, HOST_PHASE, CONNECT_PHASE, CLOSE_PHASE };
    public:
        void whatami() {
            std::cout << "I am a Transform Command." << '\n';
        }
        void handle_message(Message * message) {
            this->whatami();
            std::cout << "Handling Message..." << '\n';
            this->message = message;
            if ( message->get_phase() == 0 ) {
                /* code */
            }
        }
};

// Handle Requests regarding Connecting to a new network or peer
class Connect : public Command {
    private:
        enum Phase { CONTACT_PHASE, JOIN_PHASE, CLOSE_PHASE };
    public:
        void whatami() {
            std::cout << "I am a Connect Command." << '\n';
        }
        void handle_message(Message * message) {
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
        case NULL_CMD:
            return NULL;
            break;
    }
}


#endif
