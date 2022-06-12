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
# @file setup.py
# @brief This python script is used to install the pytorch_cpp_plugin into an existing environment that has pytorch
#
# Build Variables --
# Format: VARIABLE (type: default value): usage
#
# - DPDK (boolean: 0): Add dpdk backend specific compiler/linker options.
# - MLX5 (boolean: 0): Add dpdk backend Connect-x5/Connect-x4 specific compiler/linker options.
# - MLX4 (boolean: 0): Add dpdk backend Connect-x3 specific compiler/linker options.
# - RDMA (boolean: 0): Add rdma backend specific compiler/linker options.
# - BUILDDIR (path: dev_root/build): Where to store the generated pytorch patch.
# - SWITCHML_HOME (path: dev_root/build): Where to look for the switchml client library installation.
# - GRPC_HOME (path: dev_root/third_party/grpc/build): Where to look for the GRPC installation
# - DPDK_HOME (path: dev_root/third_party/dpdk/build): Where to look for the DPDK installation
#

import os
import sys
import subprocess
import torch
from setuptools import setup
from torch.utils import cpp_extension

# Parse environment variables
DEVROOT = os.path.abspath("../../")
BUILDDIR = f"{DEVROOT}/build"
SWITCHML_HOME = os.environ.get("SWITCHML_HOME", BUILDDIR)
DPDK_HOME = os.environ.get("DPDK_HOME", f"{DEVROOT}/third_party/dpdk/build")
GRPC_HOME = os.environ.get("GRPC_HOME", f"{DEVROOT}/third_party/grpc/build")
RDMA = os.environ.get("RDMA", "0") == "1"
DPDK = os.environ.get("DPDK", "0") == "1"
GRPC = 0
MLX5 = os.environ.get("MLX5", "0") == "1"
MLX4 = os.environ.get("MLX4", "0") == "1"
DEBUG = os.environ.get("DEBUG", "0") == "1"

# Initialize compilation lists
sources = ["ProcessGroupSML.cpp"]
include_dirs = [os.path.abspath("."), f"{SWITCHML_HOME}/include"]
libraries = ["switchml-client", "pthread", "glog", "boost_program_options"]
library_dirs = [f"{SWITCHML_HOME}/lib"]
extra_link_args = list()
extra_compile_args = list()

if DEBUG:
    extra_compile_args.extend(["-g", "-O0"])
else:
    extra_compile_args.append("-O3")

if RDMA and DPDK:
    print("Enabling both DPDK and RDMA backends is not supported.")
    exit(1)

if DPDK:
    print("DPDK is set.")
    GRPC=1
    libraries.extend(["dl", "numa"])
    include_dirs.append(f"{DPDK_HOME}/include")
    library_dirs.append(f"{DPDK_HOME}/lib")
    extra_link_args.extend(["-Wl,--whole-archive", "-ldpdk", "-Wl,--no-whole-archive"])
    
    if MLX5 and MLX4:
        print("Enabling both MLX5 and MLX4 is not allowed. Choose the one that matches your ConnectX version.")
        exit(1)

    if MLX5:
        libraries.extend(["ibverbs", "mlx5", "mnl"])
    elif MLX4:
        libraries.extend(["ibverbs", "mlx4", "mnl"])

if RDMA:
    print("RDMA is set.")
    GRPC=1
    libraries.extend(["ibverbs", "hugetlbfs"])

if GRPC == 1:
    grpc_compiler_instructions = {
        "inc_dirs": ("--cflags-only-I", "-I"),
        "lib_dirs": ("--libs-only-L", "-L"),
        "libs": ("--libs-only-l", "-l")
    }
    env_with_cfg_path = os.environ.copy() 
    env_with_cfg_path["PKG_CONFIG_PATH"] = f"{GRPC_HOME}/lib/pkgconfig/"
    for gci, (pkgcfg, prefix) in grpc_compiler_instructions.items():
        r = subprocess.run(f"pkg-config {pkgcfg} protobuf grpc++".split(" "),
                           stdout=subprocess.PIPE, text=True, env=env_with_cfg_path)
        output = r.stdout
        grpc_compiler_instructions[gci] = [x.replace(prefix, "").strip() for x in output.split(" ")]

    include_dirs.extend(grpc_compiler_instructions["inc_dirs"])
    library_dirs.extend(grpc_compiler_instructions["lib_dirs"])
    libraries.extend(grpc_compiler_instructions["libs"])
    

if torch.cuda.is_available():
    module = cpp_extension.CUDAExtension
else:
    module = cpp_extension.CppExtension

module = module(
    name = "torch_switchml",
    sources = sources,
    include_dirs = include_dirs,
    library_dirs = library_dirs,
    libraries = libraries,
    extra_compile_args=extra_compile_args,
    extra_link_args=extra_link_args
)

setup(
    name = "torch_switchml",
    version = "0.1",
    ext_modules = [module],
    cmdclass={'build_ext': cpp_extension.BuildExtension}
)