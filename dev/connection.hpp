#ifndef CONNECTION_HPP
#define CONNECTION_HPP

#include "utility.hpp"

#include "zmq.hpp"

class Connection {
    protected:
        bool active;

    public:
        // Method method;
        virtual void whatami() = 0;
        void unspecial() {
            std::cout << "I am calling an unspecialized function." << '\n';
        }
};

class Direct : public Connection {
    public:
        void whatami() {
            std::cout << "I am a Direct implementation." << '\n';
        }
};

class Bluetooth : public Connection {
    public:
        void whatami() {
            std::cout << "I am a BLE implementation." << '\n';
        }
};

class LoRa : public Connection {
    public:
        void whatami() {
            std::cout << "I am a LoRa implementation." << '\n';
        }
};

class Websocket : public Connection {
    public:
        void whatami() {
            std::cout << "I am a Websocket implementation." << '\n';
        }
};

inline Connection* ConnectionFactory(TechnologyTypes technology) {
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
}

#endif
