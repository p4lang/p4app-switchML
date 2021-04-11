# SwitchML Project
# @file examples makefile
# @brief A single makefile to compile all of the examples
# 
# Compilation flags --
# Format: FLAG (default value): usage
#
# - DEBUG (0): Disable optimizations, add debug symbols.
# - CUDA (0): Add the gpu device option for the allreduce benchmark.
# - DPDK (0): Set if the client library was compiled with DPDK support.
# - MLX5 (0): Set if you are using Mellanox ConnectX-5
# - MLX4 (0): Set if you are using Mellanox ConnectX-4
#

BUILDDIR ?= ../build
SWITCHML_HOME ?= $(BUILDDIR)
LIBDIR ?= $(SWITCHML_HOME)/lib
INCDIR ?= $(SWITCHML_HOME)/include
BINDIR ?= $(BUILDDIR)/bin
OBJDIR ?= $(BUILDDIR)/obj
SRCDIR ?= .

CXXFLAGS += -std=c++17
LDFLAGS += -L$(LIBDIR)
LDFLAGS += -lswitchml-client -lglog -lstdc++ -lboost_program_options -lpthread

ifeq ($(DEBUG),1)
$(info DEBUG is set. Enabling all compiler warnings, adding debug symbols, and disabling optimizations.)
CXXFLAGS += -DDEBUG -Wall -Wextra -g -O0
else
$(info DEBUG is not set. Enabling all optimizations.)
CXXFLAGS += -DNDEBUG -O3
endif

ifeq ($(DPDK), 1)
$(info DPDK is set.)
LDFLAGS += -L../third_party/dpdk/build/lib -ldl -lnuma
ifeq ($(MLX5),1)
$(info MLX5 is set.)
LDFLAGS += -libverbs -lmlx5 -lmnl
else
ifeq ($(MLX4),1)
$(info MLX4 is set.)
LDFLAGS += -libverbs -l mlx4 -l mnl
endif
endif
# Dpdk must be included as a whole archive or otherwise dpdk does not detect the different network devices
LDFLAGS := -Wl,--whole-archive -ldpdk -Wl,--no-whole-archive $(LDFLAGS) 
endif

INC += -I$(INCDIR)

.phony: default
default: hello_world

hello_world: $(BINDIR)/hello_world


$(BINDIR)/hello_world: $(BINDIR)
	$(CXX) $(CXXFLAGS) $(INC) $(SRCDIR)/hello_world/main.cc $(LDFLAGS) -o $(BINDIR)/hello_world 

$(BINDIR):
	mkdir -p $(BINDIR)

.phony: clean
clean:
	$(RM) $(BINDIR)/hello_world
