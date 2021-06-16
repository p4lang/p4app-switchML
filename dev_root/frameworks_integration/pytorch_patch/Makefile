# SwitchML Project
# @file pytorch_patch makefile
# @brief This makefile is used to make the pytorch patch
#
# Compilation flags --
# Format: FLAG (default value): usage
#
# - DPDK (0): Add dpdk backend specific compiler/linker options.
# - MLX5 (0): Add dpdk backend Connect-x5 specific compiler/linker options.
# - MLX4 (0): Add dpdk backend Connect-x4 specific compiler/linker options.
# - RDMA (0): Add rdma backend specific compiler/linker options.
#

DEVROOT := $(realpath ../../)
BUILDDIR ?= $(DEVROOT)/build
SWITCHML_HOME ?= $(BUILDDIR)
DPDK_HOME ?= $(DEVROOT)/third_party/dpdk/build
GRPC_HOME ?= $(DEVROOT)/third_party/grpc/build

INCDIRS = -I$(SWITCHML_HOME)/include
LINKLIBS = -lswitchml-client -lpthread -lglog -lboost_program_options
LINKDIRS = -L$(SWITCHML_HOME)/lib 

ifeq ($(RDMA),1)
ifeq ($(DPDK),1)
$(error Enabling both DPDK and RDMA backends is not supported.)
endif
endif

ifeq ($(DPDK), 1)
$(info DPDK is set.)
COMPILATION_FLAGS += DPDK=1
LINKLIBS += -ldl -lnuma
INCDIRS += -I$(DPDK_HOME)/include

ifeq ($(MLX5),1)
$(info MLX5 is set.)
COMPILATION_FLAGS += MLX5=1
LINKLIBS += -libverbs -lmlx5 -lmnl
else
ifeq ($(MLX4),1)
$(info MLX4 is set.)
COMPILATION_FLAGS += MLX4=1
LINKLIBS += -libverbs -lmlx4 -lmnl
endif
endif

# Dpdk must be included as a whole archive for it to work properly.
LINKDIRS += -L$(DPDK_HOME)/lib
LINKLIBS := -Wl,--whole-archive -ldpdk -Wl,--no-whole-archive $(LINKLIBS) 
endif

ifeq ($(RDMA), 1)
$(info RDMA is set.)
COMPILATION_FLAGS += RDMA=1
GRPC=1
LINKLIBS += -libverbs -lhugetlbfs
endif

ifeq ($(GRPC),1)
INCDIRS += $(shell PKG_CONFIG_PATH=$(GRPC_HOME)/lib/pkgconfig/ pkg-config --cflags-only-I protobuf grpc++)
LINKDIRS += $(shell PKG_CONFIG_PATH=$(GRPC_HOME)/lib/pkgconfig/ pkg-config --libs-only-L protobuf grpc++)
LINKLIBS += $(shell PKG_CONFIG_PATH=$(GRPC_HOME)/lib/pkgconfig/ pkg-config --libs-only-l protobuf grpc++)
endif

NO_PREFIX_INCDIRS = $(INCDIRS:-I%=%)
NO_PREFIX_LINKDIRS = $(LINKDIRS:-L%=%)
NO_PREFIX_LINKLIBS = $(LINKLIBS:-l%=%)

SED_ESCAPED_INCDIRS := $(shell printf '%s\n' "$(NO_PREFIX_INCDIRS)" | sed -e 's/[\/&]/\\&/g')
SED_ESCAPED_LINKDIRS := $(shell printf '%s\n' "$(NO_PREFIX_LINKDIRS)" | sed -e 's/[\/&]/\\&/g')
SED_ESCAPED_LINKLIBS := $(shell printf '%s\n' "$(NO_PREFIX_LINKLIBS)" | sed -e 's/[\/&]/\\&/g')

$(BUILDDIR)/switchml_pytorch.patch: $(BUILDDIR)
	cp switchml_pytorch.patch $(BUILDDIR)/

	sed -i 's/#SWITCHML_COMPILATION_FLAGS#/$(COMPILATION_FLAGS)/' $(BUILDDIR)/switchml_pytorch.patch

	sed -i 's/$$ENV{SWITCHML_INC_DIR}/$(SED_ESCAPED_INCDIRS)/' $(BUILDDIR)/switchml_pytorch.patch
	sed -i 's/$$ENV{SWITCHML_LINK_DIR}/$(SED_ESCAPED_LINKDIRS)/' $(BUILDDIR)/switchml_pytorch.patch
	sed -i 's/$$ENV{SWITCHML_LINK_LIB}/$(SED_ESCAPED_LINKLIBS)/' $(BUILDDIR)/switchml_pytorch.patch

	@echo you will find the complete pytorch patch in the build directory.

.PHONY: clean
clean:
	rm -f $(BUILDDIR)/switchml_pytorch.patch

$(BUILDDIR):
	mkdir -p $(BUILDDIR)