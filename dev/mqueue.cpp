#define len_index 7
mqueue::mqueue(){
	this->in_msg = new std::vector<Message>;
	this->out_msg = new std::vector<Message>;
	this->pipes = new std::vector<std::string>;
}
mqueue::mqueue(std::vector<std::string> setupPipes){
	this->pipes = setupPipes;
	this->in_msg = new std::vector<Message>;
	this->out_msg = new std::vector<Message>;
	
}
mqueue::~mqueue(){
	delete this->pipes;
	delete this->in_msg;
	delete this->out_msg;
}
void addPipe(std::string newPipe){
	pipes.push_back(newPipe);
}
void removePipe(std::string badPipe){
	int i;
	for(i = 0; i<pipes.size(); i++){
		if(pipes[i].compare(badPipe)!=0){
			pipes.erase(pipes.begin()+i);
		}
	}
}
int pushPipes(){//finds *inbound* msgs
	int i;
	int start_size = in_msg.size();
	for(i = 0; i<start_size; i++){
		tmp_msg = in_msg[0];
		const std::string pipe_name = tmp_msg.node_id;
		tmp_msg.pack();
		const std::string raw_top = tmp_msp.raw;
		if((int fd = open(pipe_name, O_APPEND)==-1) return 1;
		write(fd, raw_top, raw_top, raw_top.length());
		in_msg.erase(in_msg[0]);
	}

	return 0;
}
void checkPipes(){//finds *outbound* msgs
	int i;
	for(i = 0; i<pipes.size(); i++){
		const std::string = pipes[i];
		if((int fd = open(pipe_name, O_RDONLY|O_TRUNCATE)==-1) return 1;
		
		const std::string data_len = read(fd, //TODO: Ask Vince about variable length before packet_len w/ 11 nodes in cluster
	}
}
int handle_next_message(){
	if(out_msg.size()<1){
		return 1;
	}
	top_msg = this->out_msg[0];
	this->out_msg.erase(this->out_msg[0]);
	//TODO things based on top_msg
	return 0;
}
