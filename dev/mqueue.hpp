#ifndef MQUEUE_HPP
#define MQUEUE_HPP

#include "utility.hpp"

class MessageQueue {
    private:
        //std::vector<Message> messages;
        std::vector<Message> in_msg;
        std::vector<Message> out_msg;
        std::vector<std::string> pipes;

    public:
        void push_pipe();	//Sends all out_msgs to
        void check_pipes();	//Checks each FD for new incoming msgs
        void add_pipe();
        void remove_pipe();	//nuke everything
        Message get_next_message();	//return a msg from out_msg and delete after service

        MessageQueue();
        ~MessageQueue();
};
#endif
