CXX = g++-9
BUILDDIR = build
BINARYDIR = bin
BINARYNAME = BryptNode
TARGET = $(BINARYDIR)/$(BINARYNAME)

OPENSSL_LIB = -L/usr/local/opt/openssl@1.1/lib
OQS_LIB = -L/usr/local/lib

OPENSSL_INCLUDE = -I/usr/local/opt/openssl@1.1/include/openssl
OQS_INCLUDE = -I/usr/local/include/oqs
EXTERNAL_LIBARIES_INCLUDE = -ILibraries

mode = release
ifeq ($(mode),debug)
CXXFLAGS = -g3 -ggdb3 -pedantic -Wall -Wextra -Wformat-security -std=c++17 -Wno-vla -pthread  -MMD -MP
else
CXXFLAGS = -O2 -pedantic -Wall -Wextra -Wformat-security -std=c++17 -Wno-vla -pthread -MMD -MP
endif

CXXINCLUDES = $(OPENSSL_INCLUDE) $(OQS_INCLUDE) $(EXTERNAL_LIBARIES_INCLUDE)
CXXLIBS = -lzmq $(OPENSSL_LIB) $(OQS_LIB) -lpthread -lcrypto -loqs -ldl

SRCDIR = .

BRYPTNODEDIR = BryptNode
BRYPTIDENTIFIERDIR = BryptIdentifier
BRYPTMESSAGEDIR = BryptMessage
BRYPTPEERDIR = Components/BryptPeer
AWAITDIR = Components/Await
COMMANDDIR = Components/Command
ENDPOINTSDIR = Components/Endpoints
CONFIGURATIONDIR = Configuration
MESSAGECONTROLDIR = Components/MessageControl
SECURITYDIR = Components/Security
NOTIFIERDIR = Components/Notifier

MODULES = $(BRYPTNODEDIR) \
$(BRYPTIDENTIFIERDIR) \
$(BRYPTMESSAGEDIR) \
$(BRYPTPEERDIR) \
$(AWAITDIR) \
$(COMMANDDIR) \
$(ENDPOINTSDIR) \
$(CONFIGURATIONDIR) \
$(MESSAGECONTROLDIR) \
$(SECURITYDIR) \
$(NOTIFIERDIR)

SRCS = $(foreach sdir,$(MODULES),$(wildcard $(sdir)/*.cpp))
OBJS = $(patsubst %.cpp,$(BUILDDIR)/%.o,$(notdir $(SRCS)))
DEPS = $(OBJS:%.o=%.d)
#-include $(DEPS)

RMBUILDDIR = rm -r $(BUILDDIR)
RMBINARYDIR = rm -r $(BINARYDIR)

vpath %.cpp $(MODULES)

define cxx-command
$1/%.o: %.cpp
	@echo Compiling $$<...
	$(CXX) -c $$< -o $$@ $(CXXINCLUDES) $(CXXFLAGS)
	@echo
endef

.PHONY: all introduction clean clean-build-dir clean-binary-dir

all: introduction $(TARGET)

rebuild: clean all

$(TARGET): $(OBJS)
	@echo Building $(BINARYNAME) binary...
	@mkdir -p $(BINARYDIR)
	$(CXX) $^ -o $@ $(CXXINCLUDES) $(CXXFLAGS) $(CXXLIBS)
	@echo
ifdef cleanup
	@echo Removing temporary build directory...
	-$(RMBUILDDIR)
	@echo
endif

introduction: $(BUILDDIR)
	@echo Building $(BINARYNAME) to ./$(BINARYDIR)
	@echo Source Directories Found:
	@$(foreach module, $(MODULES), echo "\t-- $(module)";) echo

$(BUILDDIR):
	@mkdir -p $@

$(foreach bdir, $(BUILDDIR),$(eval $(call cxx-command,$(bdir))))

clean:
	@echo Cleaning build components...
	rm -f *.out *.o *.d
	-$(RMBUILDDIR)
	-$(RMBINARYDIR)
	@echo