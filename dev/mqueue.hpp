#ifndef MQUEUE_HPP
#define MQUEUE_HPP

#include <cstdio>
#include <fstream>
#include <string>
#include <vector>
#include <queue>

#include "utility.hpp"
#include "message.hpp"

class MessageQueue {
    private:
        std::queue<Message> in_queue;
        std::queue<Message> out_queue;

        std::vector<std::string> pipes;

    public:
        int push_pipes();	//Sends all out_queues to
        int check_pipes();	//Checks each FD for new incoming msgs
    	void add_message(std::string destination_id, Message message);
    	void push_pipe(std::string filename);
    	void remove_pipe(std::string filename);	//nuke everything
    	Message pop_next_message();	//return a msg from out_queue and delete after service

    	MessageQueue();
    	MessageQueue(std::vector<std::string> setup_pipes);
    	~MessageQueue();
};
#endif
