#include "mqueue.hpp"
#include <algorithm>

#define len_index 7

MessageQueue::MessageQueue(){
	this->in_msg = std::vector<Message>();
	this->out_msg = std::vector<Message>();
	this->pipes = std::vector<std::string>();
}

MessageQueue::MessageQueue(std::vector<std::string> setup_pipes){
	this->pipes = setup_pipes;
	this->in_msg = std::vector<Message>();
	this->out_msg = std::vector<Message>();

}

MessageQueue::~MessageQueue(){
	this->pipes.clear();
	this->in_msg.clear();
	this->out_msg.clear();
}

void MessageQueue::push_pipe(std::string filename){
	std::ifstream myFile(filename);

	if( myFile.fail() ){
		std::ofstream outfile(filename);
		outfile.close();
	}

	this->pipes.push_back(filename);

}

void MessageQueue::remove_pipe(std::string filename){
	unsigned int i;

	for(i = 0; i < pipes.size(); i++){

		if( pipes[i].compare( filename ) != 0 ) {
			remove( filename.c_str() );
			pipes.erase( pipes.begin() + i );
		}

	}

}

void MessageQueue::add_message(Message message){
	std::string pipe_name = message.get_node_id();

	if ( std::find( pipes.begin(), pipes.end(), pipe_name ) == pipes.end() ){
		std::cout << "adinmsg making new pipe?\n";
		this->push_pipe( pipe_name );
	}

	in_msg.push_back( message );

}

int MessageQueue::push_pipes(){//finds *inbound* msgs
	unsigned int i;
	unsigned int start_size = in_msg.size();

	this->check_pipes();

	std::string debugstring = "";
	std::string packet = "";

	for( i = 0; i < start_size; i++ ) {

		Message tmp_msg = in_msg[i];
		const std::string pipe_name = tmp_msg.get_node_id();
		packet = tmp_msg.get_pack();
		//const std::string raw_top = tmp_msg.get_raw();

		std::string raw_top = "placeholder";
		std::fstream myfile(pipe_name, std::ios::out | std::ios::binary);

		for(int i = 0; i < (int)packet.size(); i++){
			myfile.put(packet.at(i));
			debugstring.append(sizeof(packet.at(i)), packet.at(i));
		}

		myfile.close();
		std::cout << "debug string was \n" << debugstring << '\n';
		debugstring = "";
		in_msg.erase( in_msg.begin() );

	}

	return 0;
}

int MessageQueue::check_pipes(){//finds *outbound* msgs
	std::string line = "";
	char tmpchar;

	for(int i = 0; i < (int)pipes.size(); i++){

		const char* pipe_name = pipes[i].c_str();
		std::ifstream myfile(pipe_name);
		// printf("opening\n");

		if( myfile.is_open() ){

			while( myfile.get( tmpchar ) ){
				line.append( sizeof( tmpchar ),tmpchar );
			}

			try {
				// TODO: Clear pipe file contents
				Message tmpmsg(line);
				out_msg.push_back(tmpmsg);
			} catch(...) {
				std::cout << "== [Message Queue] Message in queue not formatted properly" << '\n';
			}

		}

		myfile.close();

		std::fstream truncator(pipe_name, std::ios::in);
		truncator.open( pipe_name, std::ios::out | std::ios::trunc );
		truncator.close();

		// std::cout << "\nMessage get_pack\n" << out_msg[0].get_pack() << '\n';

	}

	return 0;
}

/*
 * IMPORTANT USAGE NOTE: pop_... will return an empty message if none
 * are available. Check the return value's command phase for -1 to see
 * if retruned message is real
 */

Message MessageQueue::pop_next_message(){

	if( out_msg.size() > 0 ) {

		Message top_msg = this->out_msg[0];
		out_msg.erase(out_msg.begin());
		return top_msg;

	} else {

		Message tmpmsg = Message();
		return tmpmsg;

	}

}
