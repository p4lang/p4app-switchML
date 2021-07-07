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
# Build Variables --
# Format: VARIABLE (type: default value): usage
#
# - DEBUG (boolean: 0): Disable optimizations, add debug symbols.
# - DPDK (boolean: 0): Add dpdk backend specific compiler/linker options.
# - MLX5 (boolean: 0): Add dpdk backend Connect-x5/Connect-x4 specific compiler/linker options.
# - MLX4 (boolean: 0): Add dpdk backend Connect-x3 specific compiler/linker options.
# - RDMA (boolean: 0): Add rdma backend specific compiler/linker options.
# - BUILDDIR (path: dev_root/build): Where to store generated objects/binaries.
# - SWITCHML_HOME (path: dev_root/build): Where to look for the switchml client library installation.
# - GRPC_HOME (path: dev_root/third_party/grpc/build): Where to look for the GRPC installation
# - DPDK_HOME (path: dev_root/third_party/dpdk/build): Where to look for the DPDK installation
#

# Init path variables
DEVROOT := $(realpath ../)
BUILDDIR ?= $(DEVROOT)/build
SWITCHML_HOME ?= $(BUILDDIR)
LIBDIR ?= $(SWITCHML_HOME)/lib
INCDIR ?= $(SWITCHML_HOME)/include
BINDIR ?= $(BUILDDIR)/bin
OBJDIR ?= $(BUILDDIR)/obj
SRCDIR ?= $(DEVROOT)/examples

# Compiler / linker flags
CXXFLAGS += -std=c++17
LDFLAGS += -L$(LIBDIR)
LDFLAGS += -lswitchml-client -lglog -lstdc++ -lboost_program_options -lpthread
INC += -I$(INCDIR)

# Parse compilation options -----
ifeq ($(DEBUG),1)
$(info DEBUG is set. Enabling all compiler warnings, adding debug symbols, and disabling optimizations.)
CXXFLAGS += -DDEBUG -Wall -Wextra -g -O0
else
$(info DEBUG is not set. Enabling all optimizations.)
CXXFLAGS += -DNDEBUG -O3
endif

ifeq ($(RDMA),1)
ifeq ($(DPDK),1)
$(error Enabling both DPDK and RDMA backends is not supported.)
endif
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

# Targets

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
