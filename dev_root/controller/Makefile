# SwitchML Project
# @file protos makefile
# @brief The makefile to generate the protobuf/gRPC server files for python.

PYTHON=python3
CONTROLLER_PATH=../controller

all:
	$(PYTHON) -m grpc_tools.protoc -I . --python_out=$(CONTROLLER_PATH) --grpc_python_out=$(CONTROLLER_PATH) switchml.proto
