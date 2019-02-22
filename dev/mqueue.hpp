#ifndef MQUEUE_HPP
#define MQUEUE_HPP

#include <cstdio>
#include <cstdlib>
#include <cmath>
#include <ctime>
#include <iostream>
#include <fstream>
#include <string>
#include <vector>

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <ifaddrs.h>
#include <net/if.h>
#include <netdb.h>
#include <arpa/inet.h>

#include "utility.hpp"
#include "connection.hpp"
#include "message.hpp"
#include "command.hpp"
#include "node.hpp"

class MessageQueue {
    private:
        //std::vector<Message> messages;
        std::vector<Message> in_msg;
	std::vector<Message> out_msg;
        std::vector<std::string> pipes;
        
    public:
        int pushPipes();	//Sends all out_msgs to 
        int checkPipes();	//Checks each FD for new incoming msgs
	void addInMessage(Message);
	void addPipe(std::string);
	void removePipe(std::string);	//nuke everything
	Message get_next_message();	//return a msg from out_msg and delete after service
	
	MessageQueue();
	MessageQueue(std::vector<std::string>);
	~MessageQueue();
};
#endif
