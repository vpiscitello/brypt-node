CXX = g++
TARGET = device

LZMQ = -lzmq
LPTHREAD = -lpthread
LOPENSSL_LIB = -L/usr/local/opt/openssl/lib
LOPENSSL_INCLUDE = -L/usr/local/opt/openssl/include/openssl
LCRYPTO = -lcrypto

CXXLIBS = $(LZMQ) $(LPTHREAD) $(LOPENSSL_LIB) $(LOPENSSL_INCLUDE) $(LCRYPTO)
CXXFLAGS = -O2 -pedantic -Wall -Wextra -Wformat-security -std=c++11

$(TARGET): test.o json11.o mqueue.o node.o command.o watcher.o
	$(CXX) test.o json11.o mqueue.o node.o command.o watcher.o  -o $(TARGET) $(CXXFLAGS) $(CXXLIBS)

test.o: test.cpp
	$(CXX) -c test.cpp $(CXXFLAGS)

node.o: node.cpp
	$(CXX) -c node.cpp $(CXXFLAGS)

json11.o: json11.cpp
	$(CXX) -c json11.cpp $(CXXFLAGS)

mqueue.o: mqueue.cpp
	$(CXX) -c mqueue.cpp $(CXXFLAGS)

command.o: command.cpp
	$(CXX) -c command.cpp $(CXXFLAGS)

watcher.o: watcher.cpp
	$(CXX) -c watcher.cpp $(CXXFLAGS)

run:
	./$(TARGET)

clean:
	rm -f *.out *.o $(TARGET)
