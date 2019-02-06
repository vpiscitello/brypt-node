#ifndef MQUEUE_HPP
#define MQUEUE_HPP

#include "utility.hpp"

class MessageQueue {
    private:
        std::vector<Message> messages;
        std::vector<std::string> pipes;
        
    public:
        void push_pipe();
        void check_pipes();
        void handle_next_message();

};

#endif
