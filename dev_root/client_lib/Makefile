# SwitchML Project
# @file client_lib makefile
# @brief The makefile to compile the client library.
#
# Compilation flags --
# Format: FLAG (default value): usage
#
# - DEBUG (0): Disable optimizations, add debug symbols, and enable detailed debugging messages.
# - DPDK (0): Compile and include the dpdk backend.
# - RDMA (0): Compile and include the rdma backend.
# - DUMMY (1): Compile and include the dummy backend.
# - VCL (1): Compile with the vector class library (Used to speedup prepostprocessing on the CPU)
# - TIMEOUTS (1): Compile with timeouts and retransmissions support.
#
# Note: we postfix most variables with _ to avoid collisions with DPDK's build system

# Init path variables
MKDIR_ := $(dir $(realpath $(firstword $(MAKEFILE_LIST)))).
BUILDDIR ?= $(MKDIR_)/../build
INCDIR_ := $(BUILDDIR)/include/switchml
LIBDIR_ := $(BUILDDIR)/lib
OBJDIR_ := $(BUILDDIR)/obj
SRCDIR_ ?= $(MKDIR_)/src
SRCDIRS_ := $(SRCDIR_) $(SRCDIR_)/prepostprocessors $(SRCDIR_)/schedulers # The directories that we include by default.
CFGFILES := $(SRCDIR_)/configs/general.cfg
LIB ?= libswitchml-client.a # Name constraint from DPDK

ifeq ($(DPDK),1)
RTE_SDK = $(MKDIR_)/../third_party/dpdk
RTE_TARGET = build
include $(RTE_SDK)/mk/rte.vars.mk
endif

# Compiler / linker flags
CXXFLAGS += -std=c++17 -fPIC
LDFLAGS += -lboost_program_options -lglog -lpthread -lstdc++

# Parse compilation options -----

ifeq ($(DEBUG),1)
$(info DEBUG is set. Enabling all compiler warnings, adding debug symbols, and disabling optimizations.)
CXXFLAGS += -DDEBUG -Wall -Wextra -g -O0
else
$(info DEBUG is not set. Enabling all optimizations.)
CXXFLAGS += -DNDEBUG -O3 
endif

ifeq ($(DUMMY),0)
ifneq ($(DPDK),1)
ifneq ($(RDMA),1)
$(error You must enable at least one backend)
endif
endif
endif

ifeq ($(DPDK),1)

$(info DPDK set. Compiling the dpdk backend..)
CXXFLAGS += -DDPDK
SRCDIRS_ += $(SRCDIR_)/backends/dpdk
CFGFILES += $(SRCDIR_)/configs/dpdk.cfg

ifneq ($(TIMEOUTS),0)
$(info TIMEOUTS set. Compiling with retransmissions)
CXXFLAGS += -DTIMEOUTS
CFGFILES += $(SRCDIR_)/configs/dpdk_timeouts.cfg
endif

endif

ifeq ($(RDMA),1)
$(error RDMA set (Not implemented yet))
CXXFLAGS += -DRDMA
SRCDIRS_ += $(SRCDIR_)/backends/rdma
CFGFILES += $(SRCDIR_)/configs/rdma.cfg
endif

ifneq ($(DUMMY),0)
$(info DUMMY set. Compiling the dummy backend..)
CXXFLAGS += -DDUMMY
SRCDIRS_ += $(SRCDIR_)/backends/dummy
CFGFILES += $(SRCDIR_)/configs/dummy.cfg
endif

ifneq ($(VCL), 0)
$(info VCL set. Compiling with vector instructions support)
CXXFLAGS += -DVCL -I $(MKDIR_)/../third_party/
ifeq ($(DEBUG), 1)
$(warning VCL may cause problems if it was compiled with the DEBUG flag and without optimizations )
endif
endif


OBJDIRS_ := $(SRCDIRS_:$(SRCDIR_)%=$(OBJDIR_)%)
CXXFLAGS += $(shell printf '-I %s ' $(SRCDIRS_)) # add includes

# Source files
SRCS-absolute_ := $(shell find $(SRCDIRS_) -maxdepth 1 -name "*.cc" )
SRCS_ := $(shell realpath --relative-to="$(SRCDIR_)" $(SRCS-absolute_))
SRCS-y := $(SRCS-absolute_) # Name constraint from DPDK

# Header files
HDRS-absolute_ := $(shell find $(SRCDIRS_) -maxdepth 1 -name "*.h" )
HDRS_ := $(shell realpath --relative-to="$(SRCDIR_)" $(HDRS-absolute_))

# Object files
OBJS_ := $(SRCS_:%.cc=$(OBJDIR_)/%.o)

# Version info
# This information is to be included in the compiled library
# and printed when the context is created
GIT_REMOTE=$(shell git remote get-url --push origin)
GIT_HASH=$(shell git rev-parse HEAD)
GIT_BRANCH=$(shell git branch | grep "^\*" | sed 's/^..//')
GIT_STATUS=$(shell git status --short)
VERSION_INFO := '"CXXFLAGS=$(CXXFLAGS) GIT_REMOTE=$(GIT_REMOTE) GIT_BRANCH=$(GIT_BRANCH) GIT_HASH=$(GIT_HASH) GIT_STATUS=$(GIT_STATUS)"'
CXXFLAGS += -DVERSION_INFO=$(VERSION_INFO)

# Targets

.PHONY: default_
ifeq ($(DPDK), 1)
# With DPDK the DPDK Makefiles are the ones that actually handle the compilation
# of our source files. So we do not need to compile them ourselves.
# Though it does use the standard variable names such as CXXFLAGS and LDFLAGS when 
# it compiles our sources
default_: _postbuild $(INCDIR_) $(LIBDIR_) config_
	$(Q)$(MAKE) clean
	cp $(RTE_OUTPUT)/$(LIB) $(LIBDIR_)/
	$(RM) -r $(RTE_OUTPUT)
	$(RM) _postclean
else
# If we're not using DPDK then we need to compile the library ourselves.
default_: $(LIBDIR_)/$(LIB) $(INCDIR_) config_
endif
# Copy headers
	cd $(SRCDIR_) && cp --parents $(HDRS_) $(INCDIR_)

$(LIBDIR_)/$(LIB): $(OBJS_) $(LIBDIR_)
	$(AR) rcs $(LIBDIR_)/$(LIB) $(OBJS_)

$(OBJDIR_)/%.o: $(SRCDIR_)/%.cc $(OBJDIRS_)
	$(CXX) $(CXXFLAGS) -c $< -o $@ $(LDFLAGS)

.PHONY: config_
config_:
	cat $(CFGFILES) > $(BUILDDIR)/switchml.cfg

$(OBJDIRS_):
	mkdir -p $@

$(LIBDIR_):
	mkdir -p $(LIBDIR_)

$(INCDIR_):
	mkdir -p $(INCDIR_)

ifeq ($(DPDK), 1)
include $(RTE_SDK)/mk/rte.extlib.mk
else
# DPDK has its own clean target that we use 
.phony: clean
clean:
	rm -r -f $(OBJDIR_) 
	rm -r -f $(INCDIR_)
# LIBDIR_ is used by other makefiles. Only remove what we produced
	rm -f $(LIBDIR_)/$(LIB)s
endif