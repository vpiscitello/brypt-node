CXX = g++-10
BUILDDIR = ../../build
BINARYDIR = ../../bin
BINARYNAME = UT_Configuration
TARGET = $(BINARYDIR)/$(BINARYNAME)

GTEST_INCLUDES = -isystem ../../Libraries/googletest -isystem ../../Libraries/googletest/include
LIJSON_INCLUDE = -isystem ../../Libraries/lithium
OPENSSL_INCLUDE = -isystem /usr/local/opt/openssl@1.1/include/openssl
OQS_INCLUDE = -isystem /usr/local/include/oqs

OPENSSL_LIB = -L/usr/local/opt/openssl@1.1/lib
OQS_LIB = -L/usr/local/lib

mode = release
ifeq ($(mode),debug)
CXXFLAGS = -g3 -ggdb3 -pedantic -Wall -Wextra -Wextra-semi -Wformat-security -Wnull-dereference\
 -Wduplicated-branches -Wduplicated-cond -Wconversion -fcoroutines -pthread -std=c++20 
else
CXXFLAGS = -O2 -pedantic -Wall -Wextra -Wextra-semi -Wformat-security -Wnull-dereference\
 -Wduplicated-branches -Wduplicated-cond -Wconversion -fcoroutines -pthread -std=c++20 
endif

CXXINCLUDES = $(GTEST_INCLUDES) $(LIJSON_INCLUDE) $(OPENSSL_INCLUDE) $(OQS_INCLUDE)
CXXLIBS = $(OPENSSL_LIB) $(OQS_LIB) -lpthread -lcrypto -lboost_system -loqs -ldl

SRCDIR = .

BRYPTIDENTIFIERDIR = ../../BryptIdentifier
BRYPTMESSAGEDIR = ../../BryptMessage
BRYPTPEERDIR = ../../Components/BryptPeer
CONFIGURATIONDIR = ../../Configuration
MESSAGECONTROLDIR = ../../Components/MessageControl
SECURITYDIR = ../../Components/Security
SECURITYPOSTQUANTUMDIR = ../../Components/Security/PostQuantum
GTESTDIR = ../../Libraries/googletest

MODULES = $(SRCDIR) \
$(BRYPTIDENTIFIERDIR) \
$(BRYPTMESSAGEDIR) \
$(BRYPTPEERDIR) \
$(CONFIGURATIONDIR) \
$(MESSAGECONTROLDIR) \
$(SECURITYDIR) \
$(SECURITYPOSTQUANTUMDIR) \
$(GTESTDIR)

SRCS = $(foreach sdir,$(MODULES),$(wildcard $(sdir)/*.cpp))
OBJS = $(patsubst %.cpp,$(BUILDDIR)/%.o,$(notdir $(SRCS))) $(BUILDDIR)/gtest-all.o
DEPS = $(OBJS:%.o=%.d) 

RMBUILDDIR = rm -r $(BUILDDIR)

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

$(BUILDDIR)/gtest-all.o: $(GTESTDIR)/src/gtest-all.cc
	$(CXX) $(CXXFLAGS) $(CXXINCLUDES) -c -o $@ $<

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
	@echo

