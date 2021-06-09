# SwitchML Project
# @file nccl_plugin makefile
# @brief Used to build the nccl plugin

# Init path variables
BUILDDIR ?= ../../build
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
LDFLAGS += -L../../third_party/dpdk/build/lib -ldl -lnuma
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
# TODO:
$(error The RDMA backend is not supported for the nccl plugin yet.)
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
