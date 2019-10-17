CXX = g++
TARGET = device
OBJ = $(SRC:.cpp=.o)
DEP = $(OBJ:.o=.d)
BLD = build

LZMQ = -lzmq
LPTHREAD = -lpthread
LOPENSSL_LIB = -L/usr/local/opt/openssl/lib
LOPENSSL_INCLUDE = -L/usr/local/opt/openssl/include/openssl
LCRYPTO = -lcrypto

CXXLIBS = $(LZMQ) $(LPTHREAD) $(LOPENSSL_LIB) $(LOPENSSL_INCLUDE) $(LCRYPTO)
CXXFLAGS = -O2 -pedantic -Wall -Wextra -Wformat-security -std=c++17 -Wno-vla -MMD -MP -I/Library/Developer/CommandLineTools/SDKs/MacOSX10.15.sdk/usr/include/

AWAITDIR = $(wildcard Components/Await/*.cpp)
COMMANDDIR = $(wildcard Components/Command/*.cpp)
MESSAGEQUEUEDIR = $(wildcard Components/MessageQueue/*.cpp)
PEERWATCHERDIR = $(wildcard Components/PeerWatcher/*.cpp)
NOTIFIERDIR = $(wildcard Components/PeerWatcher/*.cpp)

JSON11DIR = Libraries/Json11

$(TARGET): Test json11 MessageQueue Node Handlers PeerWatcher Notifier Await
	$(CXX) Test json11 MessageQueue Node Handlers PeerWatcher Notifier Await  -o $(TARGET) $(CXXFLAGS) $(CXXLIBS)

$(BLD)/%.o: %.cpp | $(BLD)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -o $@ -c $<

$(BLD):
	mkdir $@

define cxx-command
$(CXX) $(CXXFLAGS) $< -o $@
endef

Test: Test.cpp
	$(cxx-command)

Node: Node.cpp
	$(cxx-command)

MessageQueue: $($(MESSAGEQUEUEDIR)/SRC:%.cpp=$(BLD)/%.o)
	$(cxx-command)

Handlers: $($(COMMANDDIR)/SRC:%.cpp=$(BLD)/%.o)
	$(cxx-command)

PeerWatcher: $($(PEERWATCHERDIR)/SRC:%.cpp=$(BLD)/%.o)
	$(cxx-command)

Notifier: $($(NOTIFIER)/SRC:%.cpp=$(BLD)/%.o)
	$(cxx-command)

Await: $(AWAITDIR:%.cpp=$(BLD)/%.o)
	$(cxx-command)

json11: $($(JSON11DIR)/SRC:%.cpp=$(BLD)/%.o)
	$(cxx-command)

run:
	./$(TARGET)

root:
	./$(TARGET) --root -id 1 -type DIRECT -port 3005

leaf_two:
	./$(TARGET) --leaf -id 2 -type DIRECT -port 4005 -peer 192.168.30.1 -pp 3005

leaf_three:
	./$(TARGET) --leaf -id 3 -type DIRECT -port 5005 -peer 192.168.30.1 -pp 3005

leaf_four:
	./$(TARGET) --leaf -id 4 -type DIRECT -port 6005 -peer 192.168.30.1 -pp 3005

leaf_five:
	./$(TARGET) --leaf -id 5 -type DIRECT -port 7005 -peer 192.168.30.1 -pp 3005

leaf_six:
	./$(TARGET) --leaf -id 6 -type DIRECT -port 8005 -peer 192.168.30.1 -pp 3005

# g++ -O2 -pedantic -Wall -Wextra -Wformat-security -std=c++17 -Wno-vla -MMD -MP -o build/Await.o -c Components/Await/Await.cpp

clean:
	rm -f *.out *.o build $(TARGET)
