# SwitchML Project
# @file Main makefile
# @brief The makefile to compile and prepare all of the components of the project
#
# Compilation flags --
# Format: FLAG (default value): usage
#
# - DEBUG (0): Disable optimizations, add debug symbols, and enable detailed debugging messages.
# - DPDK (0): Compile and include the dpdk backend.
# - MLX5 (0): Set if you are using Mellanox ConnectX-4 ConnectX-5.
# - MLX4 (0): Set if you are using Mellanox ConnectX-3.
# - RDMA (0): Compile and include the rdma backend.
# - VCL (1): Compile with the vector class library (Used to speedup prepostprocessing on the CPU).
# - TIMEOUTS (1): Compile with timeouts and retransmissions support.
#

DEVROOT := $(realpath .)
# Where to store all compiled files:
BUILDDIR ?= $(DEVROOT)/build # Expand absolute path for sub makefiles

.PHONY: default
default: third_party client_lib examples benchmarks

# This target was created to conveniently rebuild the library, examples, and benchmarks after a source code change to allow quick testing.
.PHONY: rebuild
rebuild: clean client_lib examples benchmarks

.PHONY: third_party
third_party:
	$(MAKE) -C third_party

.PHONY: client_lib
client_lib:
	$(MAKE) -C client_lib/

.PHONY: examples
examples: 
	$(MAKE) -C examples/

.PHONY: benchmarks
benchmarks: 
	$(MAKE) -C benchmarks/

.PHONY: pytorch_patch
pytorch_patch: 
	$(MAKE) -C frameworks_integration/pytorch_patch/

.PHONY: nccl_plugin
nccl_plugin: 
	$(MAKE) -C frameworks_integration/nccl_plugin/

.PHONY: clean
clean:
	$(MAKE) -C client_lib clean
	$(MAKE) -C examples clean
	$(MAKE) -C benchmarks clean
	$(MAKE) -C frameworks_integration/nccl_plugin/ clean
	$(MAKE) -C frameworks_integration/pytorch_patch/ clean
	$(RM) -r $(BUILDDIR)

# We rarely need to clean third party libraries as well so we have it as a separate target
.PHONY: clean_all
clean_all: clean
	$(MAKE) -C third_party clean