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

# Where to store all compiled files:
BUILDDIR ?= $(shell pwd)/build # Expand absolute path for sub makefiles

.PHONY: default
default: third_party client_lib examples benchmarks;

.PHONY: third_party
third_party:
	$(MAKE) -C third_party

.PHONY: client_lib
client_lib:
	$(MAKE) -C client_lib/ BUILDDIR=$(BUILDDIR)

.PHONY: examples
examples: 
	$(MAKE) -C examples/ BUILDDIR=$(BUILDDIR)

.PHONY: benchmarks
benchmarks: 
	$(MAKE) -C benchmarks/ BUILDDIR=$(BUILDDIR)

.PHONY: clean
clean:
	$(MAKE) -C client_lib clean
	$(RM) -r $(BUILDDIR)

# We rarely need to clean third party libraries as well so we have it as a seprate target
.PHONY: clean_all
clean_all: clean
	$(MAKE) -C third_party clean