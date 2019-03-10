#ifndef MQUEUE_HPP
#define MQUEUE_HPP

#include <cstdio>
#include <fstream>
#include <string>
#include <vector>

#include "utility.hpp"
#include "message.hpp"

class MessageQueue {
    private:
        std::vector<Message> in_msg;
        std::vector<Message> out_msg;

        std::vector<std::string> pipes;

    public:
        int push_pipes();	//Sends all out_msgs to
        int check_pipes();	//Checks each FD for new incoming msgs
    	void add_message(Message message);
    	void push_pipe(std::string filename);
    	void remove_pipe(std::string filename);	//nuke everything
    	Message pop_next_message();	//return a msg from out_msg and delete after service

    	MessageQueue();
    	MessageQueue(std::vector<std::string> setup_pipes);
    	~MessageQueue();
};
#endif
