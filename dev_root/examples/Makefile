#  Copyright 2021 Intel-KAUST-Microsoft
#
#  Licensed under the Apache License, Version 2.0 (the "License");
#  you may not use this file except in compliance with the License.
#  You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
#  Unless required by applicable law or agreed to in writing, software
#  distributed under the License is distributed on an "AS IS" BASIS,
#  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
#  See the License for the specific language governing permissions and
#  limitations under the License.

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

DEVROOT := $(realpath ../)
BUILDDIR ?= $(DEVROOT)/build
SWITCHML_HOME ?= $(BUILDDIR)
LIBDIR ?= $(SWITCHML_HOME)/lib
INCDIR ?= $(SWITCHML_HOME)/include
BINDIR ?= $(BUILDDIR)/bin
OBJDIR ?= $(BUILDDIR)/obj
SRCDIR ?= $(DEVROOT)/examples

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
GRPC = 1
DPDK_HOME ?= $(DEVROOT)/third_party/dpdk/build
LDFLAGS += -L$(DPDK_HOME)/lib -ldl -lnuma
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

ifeq ($(RDMA), 1)
$(info RDMA is set.)
GRPC = 1
LDFLAGS += -libverbs -lhugetlbfs
endif

ifeq ($(GRPC),1)
GRPC_HOME ?= $(DEVROOT)/third_party/grpc/build
export PKG_CONFIG_PATH = $(GRPC_HOME)/lib/pkgconfig/
LDFLAGS += `pkg-config --libs protobuf grpc++`
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
