# SwitchML Project
# @file third_party makefile
# @brief The makefile to compile and prepare all needed third party libraries.
#
# Compilation flags --
# Format: FLAG (default value): usage
#
# - DEBUG: Disable optimizations and add debug symbols.
# - DPDK: Compile and include the dpdk backend.
# - MLX5: Configure DPDK for Mellanox ConnectX-4 ConnectX-5. 
# - MLX4: Configure DPDK for Mellanox ConnectX-3. 
#

ifeq ($(DEBUG),1)
$(info Compiling third_party with DEBUG)
CXXFLAGS += -DDEBUG -O0 -g
else
CXXFLAGS += -O3
endif

MLX ?= 5

.PHONY: default
ifeq ($(DPDK),1)
default: dpdk
else
default:
endif

.PHONY: dpdk
dpdk:
	cd dpdk && git reset --hard
	cd dpdk && git apply ../dpdk.patch
ifeq ($(MLX5),1)
	sed -i 's/CONFIG_RTE_LIBRTE_MLX5_PMD=n/CONFIG_RTE_LIBRTE_MLX5_PMD=y/' dpdk/config/common_base
	sed -i 's/CONFIG_RTE_LIBRTE_MLX4_PMD=y/CONFIG_RTE_LIBRTE_MLX4_PMD=n/' dpdk/config/common_base
else
ifeq ($(MLX4),1)
	sed -i 's/CONFIG_RTE_LIBRTE_MLX5_PMD=y/CONFIG_RTE_LIBRTE_MLX5_PMD=n/' dpdk/config/common_base
	sed -i 's/CONFIG_RTE_LIBRTE_MLX4_PMD=n/CONFIG_RTE_LIBRTE_MLX4_PMD=y/' dpdk/config/common_base
endif
endif
	$(MAKE) -C dpdk defconfig T=x86_64-native-linuxapp-gcc
	$(MAKE) -C dpdk EXTRA_CFLAGS="-fPIC $(CXXFLAGS)" -j

.PHONY: vcl
vcl:
	# nothing to do for vcl

.PHONY: clean
clean:
	$(RM) -r dpdk/build
