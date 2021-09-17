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
# @file benchmarks makefile
# @brief A single makefile to compile all of the benchmarks
# 
# Build Variables --
# Format: VARIABLE (type: default value): usage
#
# - DEBUG (boolean: 0): Disable optimizations, add debug symbols.
# - DPDK (boolean: 0): Add dpdk backend specific compiler/linker options.
# - MLX5 (boolean: 0): Add dpdk backend Connect-x5/Connect-x4 specific compiler/linker options.
# - MLX4 (boolean: 0): Add dpdk backend Connect-x3 specific compiler/linker options.
# - RDMA (boolean: 0): Add rdma backend specific compiler/linker options.
# - CUDA (boolean: 0): Compile benchmark with cuda support (can allocate tensors on the gpu and pass its pointers to the client library)
# - BUILDDIR (path: dev_root/build): Where to store generated objects/binaries.
# - SWITCHML_HOME (path: dev_root/build): Where to look for the switchml client library installation.
# - GRPC_HOME (path: dev_root/third_party/grpc/build): Where to look for the GRPC installation
# - DPDK_HOME (path: dev_root/third_party/dpdk/build): Where to look for the DPDK installation
#

# Init path variables
DEVROOT := $(realpath ../)
BUILDDIR ?= $(DEVROOT)/build
CUDA_HOME ?= /usr/local/cuda
SWITCHML_HOME ?= $(DEVROOT)/build
LIBDIR ?= $(SWITCHML_HOME)/lib
INCDIR ?= $(SWITCHML_HOME)/include
BINDIR ?= $(BUILDDIR)/bin
OBJDIR ?= $(BUILDDIR)/obj
AR_OBJDIR ?= $(OBJDIR)/allreduce_benchmark
SRCDIR ?= $(DEVROOT)/benchmarks

# Compiler / linker flags
NVCC ?= nvcc
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

ifeq ($(CUDA),1)
$(info CUDA is set. Compiling cuda code.)
# TODO: 
$(warning CUDA is set. But GPU memory is not yet handled by any of the prepostprocessors in the client library.)
CXXFLAGS += -DCUDA
LDFLAGS += -L $(CUDA_HOME)/lib64 -lcudart
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
LDFLAGS += -libverbs -l mlx4 -lmnl
endif
endif
# Dpdk must be included as a whole archive for it to work properly.
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
default: allreduce_benchmark

allreduce_benchmark: $(BINDIR)/allreduce_benchmark

$(BINDIR)/allreduce_benchmark: $(BINDIR) $(OBJDIR)
ifeq ($(CUDA),1)
	mkdir -p $(AR_OBJDIR)
	# Compiling source
	$(CXX) --compile $(CXXFLAGS) $(INC) -I $(CUDA_HOME)/include $(SRCDIR)/allreduce_benchmark/main.cc -o $(AR_OBJDIR)/cpu_main.o
	# Linking object
	$(NVCC) -dlink $(AR_OBJDIR)/cpu_main.o -o $(AR_OBJDIR)/gpu_main.o
	$(CXX) $(CXXFLAGS) $(AR_OBJDIR)/gpu_main.o $(AR_OBJDIR)/cpu_main.o $(LDFLAGS) -o $(BINDIR)/allreduce_benchmark 
else
	$(CXX) $(CXXFLAGS) $(INC) $(SRCDIR)/allreduce_benchmark/main.cc $(LDFLAGS) -o $(BINDIR)/allreduce_benchmark 
endif

$(OBJDIR):
	mkdir -p $@

$(BINDIR):
	mkdir -p $(BINDIR)

.phony: clean
clean:
	$(RM) $(BINDIR)/allreduce_benchmark
	$(RM) -r $(AR_OBJDIR)
	$(RM) $(BINDIR)/ppp_benchmark
