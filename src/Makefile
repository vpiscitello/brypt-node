CXX = g++
BUILDDIR = build
BINARYDIR = bin
BINARYNAME = BryptNode
TARGET = $(BINARYDIR)/$(BINARYNAME)

OPENSSL_LIB = -L/usr/local/opt/openssl/lib
OPENSSL_INCLUDE = -I/usr/local/opt/openssl/include/openssl

CXXINCLUDES = $(OPENSSL_INCLUDE)
CXXLIBS = -lzmq $(OPENSSL_LIB) -pthread -lcrypto -ldl
CXXFLAGS = -O2 -pedantic -Wall -Wextra -Wformat-security -std=c++17 -Wno-vla -MMD -MP

SRCDIR = .

AWAITDIR = Components/Await
COMMANDDIR = Components/Command
CONNECTIONDIR = Components/Connection
CONTROLDIR = Components/Control
MESSAGEQUEUEDIR = Components/MessageQueue
PEERWATCHERDIR = Components/PeerWatcher
NOTIFIERDIR = Components/Notifier

JSON11DIR = Libraries/Json11

MODULES = $(SRCDIR) $(AWAITDIR) $(COMMANDDIR) $(CONNECTIONDIR) $(CONTROLDIR) $(MESSAGEQUEUEDIR) $(PEERWATCHERDIR) $(NOTIFIERDIR) $(JSON11DIR)

SRC = $(foreach sdir,$(MODULES),$(wildcard $(sdir)/*.cpp))
OBJ = $(patsubst %.cpp,$(BUILDDIR)/%.o,$(notdir $(SRC)))

RMBUILDDIR = rm -r $(BUILDDIR)
RMBINARYDIR = rm -r $(BINARYDIR)

vpath %.cpp $(MODULES)

define cxx-command
$1/%.o: %.cpp
	$(CXX) -c $$< -o $$@ $(CXXFLAGS)
endef

.PHONY: all introduction clean clean-build-dir clean-binary-dir

all: introduction $(TARGET)

rebuild: clean all

$(TARGET): $(OBJ)
	@mkdir -p $(BINARYDIR)
	$(CXX) $^ -o $@ $(CXXINCLUDES) $(CXXFLAGS) $(CXXLIBS)
ifdef cleanup
	@echo "\nRemoving temporary build directory"
	-$(RMBUILDDIR)
endif

introduction: $(BUILDDIR)
	@echo "Building $(BINARYNAME) to ./$(BINARYDIR)\n"
	@echo "Source Directories Found:"
	@$(foreach module, $(MODULES), echo "\t-- $(module)";) echo

$(BUILDDIR):
	@mkdir -p $@

$(foreach bdir, $(BUILDDIR),$(eval $(call cxx-command,$(bdir))))

clean:
	@echo "Cleaning build components"
	rm -f *.out *.o *.d
	-$(RMBUILDDIR)
	-$(RMBINARYDIR)