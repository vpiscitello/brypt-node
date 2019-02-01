#ifndef MQUEUE_HPP
#define MQUEUE_HPP

#include "utility.hpp"

class MessageQueue {
    private:
        std::vector<Message> messages;
    public:
        void push();

};

#endif
