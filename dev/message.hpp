#ifndef MESSAGE_HPP
#define MESSAGE_HPP

#include "utility.hpp"

class Message {
    private:
        std::string raw;                // Raw string format of the message

        CommandType command;            // Command type to be run
        int phase;                      // Phase of the Command state

        std::string data;               //  Encrypted data to be sent
        std::string node_id;            // ID of the sending node
        std::string timestamp;          // Current timestamp

        Message * response;             // A circular message for the response to the current message

        unsigned int nonce;             // Current message nonce
        std::string hmac;               // HMAC of the message


    public:
        void pack() {
            std::cout << "Packing message content into raw." << '\n';
        }
        void unpack() {
            std::cout << "Unpacking raw into message content." << '\n';
        }
};

#endif
