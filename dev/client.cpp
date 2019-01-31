#include <zmq.hpp>
#include <unistd.h>
#include <string>
#include <iostream>

int main ()
{
	// Prepare our context and socket
	zmq::context_t context(1);
	zmq::socket_t socket(context, ZMQ_REQ);

	std::string port = "3001";

	std::cout << "STARTING INITIAL CONNECTION\n";
	socket.connect("tcp://localhost:" + port);

	std::string message = "HELLO";
	zmq::message_t request(message.size());
	memcpy(request.data(), message.c_str(), message.size());
	std::cout << "Sending message: " << message << "\n";
	socket.send(request);

	// Get the reply.
	zmq::message_t reply;
	socket.recv(&reply);
	std::string rpl = std::string(static_cast<char*>(reply.data()), reply.size());
	std::cout << "Received: " << rpl << "\n";

	std::string message2 = "CONNECT None";
	zmq::message_t request2(message2.size());
	memcpy(request2.data(), message2.c_str(), message2.size());
	std::cout << "Sending message: " << message2 << "\n";
	socket.send(request2);
	
	// Get the reply.
	socket.recv(&reply);
	rpl = std::string(static_cast<char*>(reply.data()), reply.size());
	std::cout << "Received: " << rpl << "\n";

	// do something with the response
	
	std::string message3 = "EOF";
	zmq::message_t request3(message3.size());
	memcpy(request3.data(), message3.c_str(), message3.size());
	std::cout << "Sending message: " << message3 << "\n";
	socket.send(request3);

	// Get the reply.
	socket.recv(&reply);
	rpl = std::string(static_cast<char*>(reply.data()), reply.size());
	std::cout << "Received: " << rpl << "\n";

	//socket.disconnect("tcp://localhost:" + port);
	//socket.close();
	sleep(2);








	// Prepare our context and socket
	zmq::context_t context2(1);
	zmq::socket_t socket2(context2, ZMQ_REQ);

	port = "3010";

	std::cout << "CONNECTING TO 3010 1 time\n";
	socket2.connect("tcp://localhost:" + port);

	std::string message5 = "HELLO";
	zmq::message_t request5(message5.size());
	memcpy(request5.data(), message5.c_str(), message5.size());
	std::cout << "Sending message: " << message5 << "\n";
	socket2.send(request5);

	// Get the reply.
	socket2.recv(&reply);
	rpl = std::string(static_cast<char*>(reply.data()), reply.size());
	std::cout << "Received: " << rpl << "\n";

	socket2.close();
	context2.close();


	//// Prepare our context and socket
	//zmq::context_t context3(1);
	//zmq::socket_t socket3(context3, ZMQ_REQ);

	//port = "3010";

	//std::cout << "CONNECTING TO 3010 2 time\n";
	//socket3.connect("tcp://localhost:" + port);

	//std::string message6 = "HELLO";
	//zmq::message_t request6(message6.size());
	//memcpy(request6.data(), message6.c_str(), message6.size());
	//std::cout << "Sending message: " << message6 << "\n";
	//socket3.send(request6);

	//// Get the reply.
	//zmq::message_t reply2;
	//socket3.recv(&reply2);
	//std::string rpl2 = std::string(static_cast<char*>(reply2.data()), reply2.size());
	//std::cout << "Received: " << rpl2 << "\n";

	//socket3.close();
	//context3.close();

	return 0;
}
