CXX = g++-9
BUILDDIR = ../../build
BINARYDIR = ../../bin
BINARYNAME = UT_Configuration
TARGET = $(BINARYDIR)/$(BINARYNAME)

OPENSSL_LIB = -L/usr/local/opt/openssl@1.1/lib
OQS_LIB = -L/usr/local/lib

OPENSSL_INCLUDE = -I/usr/local/opt/openssl@1.1/include/openssl
OQS_INCLUDE = -I/usr/local/include/oqs
GTEST_INCLUDE = -I../../Libraries/googletest/include

mode = release
ifeq ($(mode),debug)
CXXFLAGS = -g3 -ggdb3 -pedantic -Wall -Wextra -Wformat-security -std=c++17 -Wno-vla -pthread
else
CXXFLAGS = -O2 -pedantic -Wall -Wextra -Wformat-security -std=c++17 -Wno-vla -pthread
endif

CXXINCLUDES = $(OPENSSL_INCLUDE) $(OQS_INCLUDE) $(GTEST_INCLUDE)
CXXLIBS = -lzmq $(OPENSSL_LIB) $(OQS_LIB) -lpthread -lcrypto -loqs -ldl

SRCDIR = .

BRYPTIDENTIFIERDIR = ../../BryptIdentifier
BRYPTMESSAGEDIR = ../../BryptMessage
BRYPTPEERDIR = ../../Components/BryptPeer
CONFIGURATIONDIR = ../../Configuration
MESSAGECONTROLDIR = ../../Components/MessageControl
SECURITYDIR = ../../Components/Security
GTESTDIR = ../../Libraries/googletest

MODULES = $(SRCDIR) \
$(BRYPTIDENTIFIERDIR) \
$(BRYPTMESSAGEDIR) \
$(BRYPTPEERDIR) \
$(CONFIGURATIONDIR) \
$(MESSAGECONTROLDIR) \
$(SECURITYDIR) \
$(GTESTDIR)

SRCS = $(foreach sdir,$(MODULES),$(wildcard $(sdir)/*.cpp))
OBJS = $(patsubst %.cpp,$(BUILDDIR)/%.o,$(notdir $(SRCS))) $(BUILDDIR)/gtest-all.o
DEPS = $(OBJS:%.o=%.d) 

RMBUILDDIR = rm -r $(BUILDDIR)

vpath %.cpp $(MODULES)

define cxx-command
$1/%.o: %.cpp
	@echo Compiling $$<...
	$(CXX) -isystem $(GTEST_INCLUDE) -I$(GTESTDIR) -c $$< -o $$@ $(CXXINCLUDES) $(CXXFLAGS)
	@echo
endef

.PHONY: all introduction clean clean-build-dir clean-binary-dir

all: introduction $(TARGET)

rebuild: clean all

$(BUILDDIR)/gtest-all.o: $(GTESTDIR)/src/gtest-all.cc
	$(CXX) -isystem $(GTEST_INCLUDE) -I$(GTESTDIR) $(CXXINCLUDES) $(CXXFLAGS) -c -o $@ $<

$(TARGET): $(OBJS)
	@echo Building $(BINARYNAME) binary...
	@mkdir -p $(BINARYDIR)
	$(CXX) -isystem $(GTEST_INCLUDE) $^ -o $@ $(CXXINCLUDES) $(CXXFLAGS) $(CXXLIBS)
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

