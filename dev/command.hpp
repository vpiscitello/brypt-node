#ifndef COMMAND_HPP
#define COMMAND_HPP

#include "utility.hpp"

class Command {
    protected:
        int id;
        int phase;
    public:
        // Method method;
        virtual void whatami() = 0;
        void unspecial() {
            std::cout << "I am calling an unspecialized function." << '\n';
        }
};

// Handle Requests regarding Node information
class Information : public Command {
    public:
        void whatami() {
            std::cout << "I am a Information Command." << '\n';
        }
};

// Handle Requests regarding Sensor readings
class Query : public Command {
    public:
        void whatami() {
            std::cout << "I am a Read Command." << '\n';
        }
};

// Handle Requests regarding Elections
class Election : public Command {
    public:
        void whatami() {
            std::cout << "I am an Election Command." << '\n';
        }
};

// Handle Requests regarding Transforming Node type
class Transform : public Command {
    public:
        void whatami() {
            std::cout << "I am a Transform Command." << '\n';
        }
};

// Handle Requests regarding Connecting to a new network or peer
class Connect : public Command {
    public:
        void whatami() {
            std::cout << "I am a Connect Command." << '\n';
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
    }
}


#endif
