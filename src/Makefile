CXX = g++
TARGET = BryptNode
BLD = build

LZMQ = -lzmq
LPTHREAD = -lpthread
LOPENSSL_LIB = -L/usr/local/opt/openssl/lib
LOPENSSL_INCLUDE = -L/usr/local/opt/openssl/include/openssl
LCRYPTO = -lcrypto

CXXLIBS = $(LZMQ) $(LPTHREAD) $(LOPENSSL_LIB) $(LOPENSSL_INCLUDE) $(LCRYPTO)
CXXFLAGS = -O2 -pedantic -Wall -Wextra -Wformat-security -std=c++17 -Wno-vla -MMD -MP

AWAITDIR = Components/Await
COMMANDDIR = Components/Command
CONNECTIONDIR = wildcard Components/Connection
CONTROLDIR = Components/Control
MESSAGEQUEUEDIR = Components/MessageQueue
PEERWATCHERDIR = Components/PeerWatcher
NOTIFIERDIR = Components/PeerWatcher

JSON11DIR = Libraries/Json11

MODULES = $(AWAITDIR) $(COMMANDDIR) $(CONNECTIONDIR) $(CONTROLDIR) $(MESSAGEQUEUEDIR) $(PEERWATCHERDIR) $(NOTIFIERDIR) $(JSON11DIR)
BUILDDIR = $(addpredix build/,$(MODULES))

SRC = $(foreach sdir,$(MODULES),$(wildcard $(sdir)/*.cpp))
OBJ = $(patsubst %.cpp,build/%.o,$(SRC))
INCLUDES = $(addpredix -I,$(MODULES))

vpath %.cpp $(MODULES)

define cxx-command
$1/%.o: %.cpp
	$(CXX) $(INCLUDES) -c $$< -o $$@
endef

.PHONY: all checkdirs clean

all: checkdirs build/$(TARGET)

build/$(TARGET): $(OBJ)
	$(CXX) $^ -o $@

checkdirs: $(BUILDDIR)

$(BUILDDIR):
	mkdir -p $@

$(foreach bdir, $(BUILDDIR),$(eval $(call cxx-command,$(bdir))))

clean:
	rm -f *.out *.o *.d $(TARGET)
	rm -r build