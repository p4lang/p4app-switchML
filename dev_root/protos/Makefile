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
# @file Protos makefile
# @brief The makefile to generate the protobufs for the client library c++ and the controller's python.

DEVROOT := $(realpath ../)

BUILDDIR ?= $(DEVROOT)/build
PROTOSDIR ?= $(DEVROOT)/protos
GRPC_HOME ?=$(DEVROOT)/third_party/grpc/build

PROTOC ?= $(GRPC_HOME)/bin/protoc
GRPC_CPP_PLUGIN ?= $(GRPC_HOME)/bin/grpc_cpp_plugin
GRPC_PYTHON_PLUGIN ?= $(GRPC_HOME)/bin/grpc_python_plugin

PBUFH := ./src/switchml.pb.h ./src/switchml.grpc.pb.h
PBUFC := ./src/switchml.pb.cc ./src/switchml.grpc.pb.cc
PBUF := $(PBUFH) $(PBUFC)

.PHONY: all
all: cpp python

.PHONY: cpp
cpp: $(PROTOSDIR)/switchml.proto
	LD_LIBRARY_PATH=$(GRPC_HOME)/lib $(PROTOC) -I $(PROTOSDIR) --cpp_out=$(BUILDDIR) --grpc_out=$(BUILDDIR) --plugin=protoc-gen-grpc=$(GRPC_CPP_PLUGIN) $<

.PHONY: python
python: $(PROTOSDIR)/switchml.proto
	$(PROTOC) -I $(PROTOSDIR) --python_out=$(BUILDDIR) --grpc_python_out=$(BUILDDIR) --plugin=protoc-gen-grpc_python=$(GRPC_PYTHON_PLUGIN) $<

.PHONY: clean
clean:
	$(RM) $(BUILDDIR)/switchml*.pb.*
	$(RM) $(BUILDDIR)/switchml*_pb2*
