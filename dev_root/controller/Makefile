# SwitchML Project
# @file controller makefile
# @brief A makefile that calls the protos makefile to generate the needed grpc python files.

DEVROOT := $(realpath ../)

.PHONY: default
default:
	$(MAKE) -C $(DEVROOT)/protos python BUILDDIR=$(DEVROOT)/controller

.phony: clean
clean:
	$(MAKE) -C $(DEVROOT)/protos clean BUILDDIR=$(DEVROOT)/controller