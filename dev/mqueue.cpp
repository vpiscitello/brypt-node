#include "mqueue.hpp"
#include <algorithm>

MessageQueue::MessageQueue(){
	this->in_queue = std::queue<Message>();
	this->out_queue = std::queue<Message>();
	this->pipes = std::vector<std::string>();
}

MessageQueue::MessageQueue(std::vector<std::string> setup_pipes){
	this->pipes = setup_pipes;
	this->in_queue = std::queue<Message>();
	this->out_queue = std::queue<Message>();
}

MessageQueue::~MessageQueue(){
	this->pipes.clear();
}

void MessageQueue::push_pipe(std::string filename){
	std::vector<std::string>::iterator it;
	for (it = this->pipes.begin(); it != this->pipes.end(); it++) {
		if (*it != filename) {
			continue;
		}
		std::cout << "== [MessageQueue] Pipe already being watched" << '\n';
		return;
	}

	std::ifstream push_file(filename);

	std::cout << "== [MessageQueue] Pushing " << filename << '\n';

	if( push_file.fail() ){
		std::ofstream outfile(filename);
		outfile.close();
	}

	push_file.close();

	this->pipes.push_back(filename);

	std::cout << "== [MessageQueue] Pipes being watched: " << this->pipes.size() << '\n';

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

void MessageQueue::add_message(std::string destination_id, Message message){
	std::string pipe_name = "./tmp/" + destination_id + ".pipe";

	std::cout << "== [MessageQueue] MessageQueued for " << pipe_name << '\n';

	// Create new pipe if the pipe name is not found in the managed pipes
	if ( std::find( pipes.begin(), pipes.end(), pipe_name ) == pipes.end() ){
		this->push_pipe( pipe_name );
	}

	out_queue.push( message );

}

int MessageQueue::push_pipes(){//finds *inbound* msgs
	unsigned int i;
	unsigned int start_size = out_queue.size();

	this->check_pipes();

	std::string debugstring = "";
	std::string packet = "";

	for( i = 0; i < start_size; i++ ) {

		Message tmp_msg = out_queue.front();
		const std::string pipe_name = "./tmp/" + tmp_msg.get_destination_id() + ".pipe";
		packet = tmp_msg.get_pack();
		out_queue.pop();

		std::cout << "== [MessageQueue] Pushing message for " << pipe_name << '\n';

		std::fstream push_file(pipe_name, std::ios::out | std::ios::binary);

		for(int i = 0; i < (int)packet.size(); i++){
			push_file.put(packet.at(i));
			debugstring.append(sizeof(packet.at(i)), packet.at(i));
		}

		// std::this_thread::sleep_for(std::chrono::seconds(120));

		push_file.close();
		std::cout << "debug string was \n" << debugstring << '\n';
		debugstring = "";

	}

	return 0;
}

int MessageQueue::check_pipes(){//finds *outbound* msgs
	for(int idx = 0; idx < (int)pipes.size(); idx++) {

		std::string pipe_name = pipes[idx];
		std::cout << "== [MessageQueue] Checking " << pipe_name << '\n';
		std::ifstream check_file(pipe_name);

		if( check_file.good() ) {

			std::string line = "";
			char current;

			std::cout << "== [MessageQueue] ";

			while( check_file.get( current ) ){
				std::cout << (int)current << ' ';
				line.append( sizeof( current ), current );
			}


			if (line.size() < 1) {
				std::cout << "No message in checked pipe" << '\n';
				continue;
			}

			std::cout << '\n';

			try {
				Message pipe_message(line);
				in_queue.push(pipe_message);

				std::ofstream clear_file(pipe_name, std::ios::out | std::ios::trunc);
				clear_file.close();
			} catch(...) {
				std::cout << "== [MessageQueue] Message in queue not formatted properly" << '\n';
			}

		}

		check_file.close();

	}

	return 0;
}

/*
 * IMPORTANT USAGE NOTE: pop_... will return an empty message if none
 * are available. Check the return value's command phase for -1 to see
 * if retruned message is real
 */

Message MessageQueue::pop_next_message(){

	if( !in_queue.empty() ) {

		Message message = this->in_queue.front();
		this->in_queue.pop();

		std::cout << "== [MessageQueue] " << in_queue.size() << " left in incoming queue" << '\n';

		return message;

	}

	return Message();

}
