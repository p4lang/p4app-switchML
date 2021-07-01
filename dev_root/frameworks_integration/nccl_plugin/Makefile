# SwitchML Project
# @file nccl_plugin makefile
# @brief Used to build the nccl plugin
#
# Compilation flags --
# Format: FLAG (default value): usage
#
# - DEBUG (0): Disable optimizations, add debug symbols, and enable detailed debugging messages.
# - DPDK (0): Add dpdk backend specific compiler/linker options.
# - MLX5 (0): Add dpdk backend Connect-x5 specific compiler/linker options.
# - MLX4 (0): Add dpdk backend Connect-x4 specific compiler/linker options.
# - RDMA (0): Add rdma backend specific compiler/linker options.
#

# Init path variables
DEVROOT := $(realpath ../../)
BUILDDIR ?= $(DEVROOT)/build
LIBDIR := $(BUILDDIR)/lib

NCCL_HOME ?=/usr/local
CUDA_HOME ?=/usr/local/cuda
SWITCHML_HOME ?= $(BUILDDIR)

LIBNAME ?= libnccl-net.so # NCCL looks for this shared library name.

# Compiler / linker flags
CXXFLAGS += -std=c++17 -fPIC -shared
LDFLAGS += -L$(SWITCHML_HOME)/lib -L$(CUDA_HOME)/lib -L$(NCCL_HOME)/lib
LDFLAGS += -lswitchml-client -lglog -lstdc++ -lboost_program_options -lpthread
INC += -I$(CUDA_HOME)/include -I$(NCCL_HOME)/include -I$(SWITCHML_HOME)/include

ifeq ($(RDMA),1)
ifeq ($(DPDK),1)
$(error Enabling both DPDK and RDMA backends is not supported.)
endif
endif

ifeq ($(DPDK), 1)
$(info DPDK is set.)
DPDK_HOME ?= $(DEVROOT)/third_party/dpdk/build
LDFLAGS += -L $(DPDK_HOME)/lib -ldl -lnuma
ifeq ($(MLX5),1)
$(info MLX5 is set.)
LDFLAGS += -libverbs -lmlx5 -lmnl
else
ifeq ($(MLX4),1)
$(info MLX4 is set.)
LDFLAGS += -libverbs -l mlx4 -lmnl
endif
endif
# Dpdk must be included as a whole archive for it to work properly.
LDFLAGS := -Wl,--whole-archive -ldpdk -Wl,--no-whole-archive $(LDFLAGS) 
endif

# Add compilations macros

ifeq ($(DEBUG),1)
$(info "Compiling with DEBUG")
CXXFLAGS += -DDEBUG -DENABLE_TRACE -O0 -g -Wall -Wextra
else
CXXFLAGS += -DNDEBUG -O3
endif

ifeq ($(RDMA), 1)
ifneq ($(MAKECMDGOALS),clean)
# TODO:
$(error The RDMA backend is not supported for the nccl plugin yet.)
endif
endif

# Targets

.phony: default
default: $(BUILDDIR)/$(LIBNAME)

clean:
	rm -f $(BUILDDIR)/$(LIBNAME)

$(BUILDDIR)/$(LIBNAME): switchml_plugin.cc $(LIBDIR)
	$(CXX) $(INC) $(CXXFLAGS) -o $(LIBDIR)/$(LIBNAME) switchml_plugin.cc $(LDFLAGS)

$(LIBDIR):
	mkdir -p $(LIBDIR)
