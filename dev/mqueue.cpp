#include "mqueue.hpp"
#define len_index 7
MessageQueue::MessageQueue(){
	this->in_msg = std::vector<Message>();
	this->out_msg = std::vector<Message>();
	this->pipes = std::vector<std::string>();
}
MessageQueue::MessageQueue(std::vector<std::string> setupPipes){
	this->pipes = setupPipes;
	this->in_msg = std::vector<Message>();
	this->out_msg = std::vector<Message>();
	
}
MessageQueue::~MessageQueue(){
	this->pipes.clear();
	this->in_msg.clear();
	this->out_msg.clear();
}
void MessageQueue::addPipe(std::string newPipe){
	this->pipes.push_back(newPipe);
}
void MessageQueue::removePipe(std::string badPipe){
	unsigned int i;
	for(i = 0; i<pipes.size(); i++){
		if(pipes[i].compare(badPipe)!=0){
			pipes.erase(pipes.begin()+i);
		}
	}
}
int MessageQueue::pushPipes(){//finds *inbound* msgs
	unsigned int i;
	unsigned int start_size = in_msg.size();
	int fd;
	for(i = 0; i<start_size; i++){
		Message tmp_msg = in_msg[0];
		const std::string pipe_name = tmp_msg.get_node_id();
		tmp_msg.pack();
		//const std::string raw_top = tmp_msg.get_raw();
		std::string raw_top = "placeholder";
		if((fd = open((const char*)pipe_name.c_str(), O_APPEND)==-1)) return 1;
		write(fd, raw_top.c_str(), raw_top.length());
		in_msg.erase(in_msg.begin());
	}
	return 0;
}
int MessageQueue::checkPipes(){//finds *outbound* msgs
	unsigned int i;
	int fd;
	for(i = 0; i<pipes.size(); i++){
		const char* pipe_name = pipes[i].c_str();
		if((fd = open(pipe_name, O_RDONLY|O_TRUNC)==-1)) return 1;
		
		//const std::string data_len = read(fd, //TODO: Ask Vince about variable length before packet_len w/ 11 nodes in cluster
	}
	return 0;
}
Message MessageQueue::get_next_message(){
	/*
	if(out_msg.size()<1){
		return 1;
	}
	*/
	Message top_msg = this->out_msg[0];
	return top_msg;
	//this->out_msg.erase(this->out_msg.begin());
	//TODO things based on top_msg
}

/*
int MessageQueue::handle_next_message(){
	if(out_msg.size()<1){
		return 1;
	}
	top_msg = this->out_msg[0];
	this->out_msg.erase(this->out_msg.begin());
	//TODO things based on top_msg
	return 0;
}
*/
