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
# @file controller makefile
# @brief A makefile that calls the protos makefile to generate the needed grpc python files.
#
# Build Variables --
# Format: VARIABLE (type: default value): usage
#
# - GRPC_HOME (path: dev_root/third_party/grpc/build): Where to look for the GRPC installation
#

DEVROOT := $(realpath ../)

.PHONY: default
default:
	$(MAKE) -C $(DEVROOT)/protos python BUILDDIR=$(DEVROOT)/controller

.phony: clean
clean:
	$(MAKE) -C $(DEVROOT)/protos clean BUILDDIR=$(DEVROOT)/controller
