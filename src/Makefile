CXX = g++-10
BUILDDIR = build
BINARYDIR = bin
BINARYNAME = BryptNode
TARGET = $(BINARYDIR)/$(BINARYNAME)

LIJSON_INCLUDE = -isystem ../Libraries/lithium
OPENSSL_INCLUDE = -isystem /usr/local/opt/openssl@1.1/include/openssl
OQS_INCLUDE = -isystem /usr/local/include/oqs

OPENSSL_LIB = -L/usr/local/opt/openssl@1.1/lib
OQS_LIB = -L/usr/local/lib

mode = release
ifeq ($(mode),debug)
CXXFLAGS = -g3 -ggdb3 -pedantic -Wall -Wextra -Wformat-security -std=c++20 -Wno-vla -pthread -fcoroutines -MMD -MP
else
CXXFLAGS = -O2 -pedantic -Wall -Wextra -Wformat-security -std=c++20 -Wno-vla -pthread -fcoroutines -MMD -MP
endif

CXXINCLUDES = $(LIJSON_INCLUDE) $(OPENSSL_INCLUDE) $(OQS_INCLUDE)
CXXLIBS = $(OPENSSL_LIB) $(OQS_LIB) -lpthread -lcrypto -loqs -ldl

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
SECURITYPOSTQUANTUMDIR = Components/Security/PostQuantum

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
$(SECURITYPOSTQUANTUMDIR)

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
	$(CXX) $(CXXFLAGS) $(CXXINCLUDES) -c $$< -o $$@
	@echo
endef

.PHONY: all introduction clean clean-build-dir clean-binary-dir

all: introduction $(TARGET)

rebuild: clean all

$(TARGET): $(OBJS)
	@echo Building $(BINARYNAME) binary...
	@mkdir -p $(BINARYDIR)
	$(CXX) $(CXXFLAGS) $(CXXINCLUDES) $^ -o $@ $(CXXLIBS)
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