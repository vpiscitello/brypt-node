#include <zmq.hpp>
#include <unistd.h>
#include <string>
#include <iostream>

int main ()
{
	std::string port = "3001";
	//  Do 10 requests, waiting each time for a response
	for (int request_nbr = 0; request_nbr != 10; request_nbr++) {
		// Prepare our context and socket
		zmq::context_t context(1);
		zmq::socket_t socket(context, ZMQ_REQ);

		std::cout << "Connecting to server...\n";
		socket.connect("tcp://localhost:" + port);

		std::string message = "CONNECT";
		zmq::message_t request(message.size());
		memcpy(request.data(), message.c_str(), message.size());
		std::cout << "Sending message: " << message << "\n";
		socket.send(request);

		//  Get the reply.
		zmq::message_t reply;
		socket.recv(&reply);
		std::string rpl = std::string(static_cast<char*>(reply.data()), reply.size());
		std::cout << "Received: " << rpl << "\n";
		//int str_size = strlen("tcp://localhost:") + strlen(rspmsg);
		//char * connect = new char[str_size];
		//strcpy(connect, "tcp://localhost:");
		//strcat(connect, rspmsg);

		//socket.close();
		//zmq_close(socket);
		//zmq_term(context);

		//std::string connect = "tcp://localhost:" + rpl;
		//std::cout << "connection: " << connect << "\n";
		//socket.connect(connect);
		port = rpl;

		//zmq::context_t context(1);
		//zmq::socket_t socket(context, ZMQ_REQ);
		sleep(4);
	}
	return 0;
}
