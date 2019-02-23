#include "mqueue.hpp"
#include <stdio.h>
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
	std::ifstream myFile(newPipe);
	if(myFile.fail()){
		std::ofstream outfile(newPipe);
		outfile.close();
	}
	this->pipes.push_back(newPipe);
}
void MessageQueue::removePipe(std::string badPipe){
	unsigned int i;
	for(i = 0; i<pipes.size(); i++){
		if(pipes[i].compare(badPipe)!=0){
			remove(badPipe.c_str());
			pipes.erase(pipes.begin()+i);
		}
	}
}
void MessageQueue::addInMessage(Message new_msg){
	std::string pipe_name = new_msg.get_node_id();
	if (std::find(pipes.begin(), pipes.end(), pipe_name) == pipes.end()){
		std::cout << "adinmsg making new pipe?\n";
		addPipe(pipe_name);
	}
	in_msg.push_back(new_msg);
}
int MessageQueue::pushPipes(){//finds *inbound* msgs
	unsigned int i;
	unsigned int start_size = in_msg.size();
	checkPipes();
	std::string debugstring = "";
	std::string packet = "";
	for(i = 0; i<start_size; i++){
		Message tmp_msg = in_msg[i];
		const std::string pipe_name = tmp_msg.get_node_id();
		packet = tmp_msg.get_pack();
		//const std::string raw_top = tmp_msg.get_raw();
		std::string raw_top = "placeholder";
		std::fstream myfile(pipe_name, std::ios::out | std::ios::binary);
		for(int i = 0; i<packet.size(); i++){
			myfile.put(packet.at(i));
			debugstring.append(sizeof(packet.at(i)), packet.at(i));
		}
		
		myfile.close();
		std::cout << "debug string was \n" << debugstring << '\n';
		debugstring = "";
		in_msg.erase(in_msg.begin());
	}
	return 0;
}
int MessageQueue::checkPipes(){//finds *outbound* msgs
	unsigned int i;
	std::string line = "";
	char tmpchar;
	printf("%d\n",pipes.size());
	for(i = 0; i<pipes.size(); i++){
		const char* pipe_name = pipes[i].c_str();
		std::ifstream myfile(pipe_name);
		printf("opening\n");
		if(myfile.is_open()){
			while(myfile.get(tmpchar)){
				line.append(sizeof(tmpchar),tmpchar);
			}
			Message tmpmsg(line);
			out_msg.push_back(tmpmsg);
		}
		myfile.close();
		std::fstream truncator(pipe_name, std::ios::in);
		truncator.open( pipe_name, std::ios::out | std::ios::trunc );
		truncator.close();
		std::cout << "\nMessage get_pack\n" << out_msg[0].get_pack() << '\n';
		//const std::string data_len = read(fd, //TODO: Ask Vince about variable length before packet_len w/ 11 nodes in cluster
	}
	return 0;
}//message get_pack returns your raw string
/*
 * IMPORTANT USAGE NOTE: pop_... will return an empty message if none
 * are available. Check the return value's command phase for -1 to see
 * if retruned message is real 
 */
Message MessageQueue::pop_next_message(){
	/*
	if(out_msg.size()<1){
		return 1;
	}
	*/
	if(out_msg.size()>0){
		Message top_msg = this->out_msg[0];
		out_msg.erase(out_msg.begin());
		return top_msg;
	}else{
		Message tmpmsg = Message();
		return tmpmsg;
	}
	
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
