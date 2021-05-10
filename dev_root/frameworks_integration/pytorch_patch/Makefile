# SwitchML Project
# @file pytorch_patch makefile
# @brief This makefile is simply used to make the pytorch patch

BUILDDIR ?= ../../build

LIBS = switchml-client pthread glog boost_program_options

ifeq ($(DPDK), 1)
$(info DPDK is set.)
LIBS += dl numa
ifeq ($(MLX5),1)
$(info MLX5 is set.)
LIBS += ibverbs mlx5 mnl
else
ifeq ($(MLX4),1)
$(info MLX4 is set.)
LIBS += ibverbs mlx4 mnl
endif
endif
# Dpdk must be included as a whole archive for it to work properly.
LIBS := -Wl,--whole-archive dpdk -Wl,--no-whole-archive $(LIBS) 
endif

$(BUILDDIR)/switchml_pytorch.patch: $(BUILDDIR)
	cp switchml_pytorch.patch $(BUILDDIR)/
	sed -i 's/#SWITCHML_LINK_LIBRARIES_LINE#/target_link_libraries(c10d PUBLIC $(LIBS))/' $(BUILDDIR)/switchml_pytorch.patch
	@echo you will find the complete pytorch patch in the build directory.

.PHONY: clean
clean:
	rm -f $(BUILDDIR)/switchml_pytorch.patch

$(BUILDDIR):
	mkdir -p $(BUILDDIR)