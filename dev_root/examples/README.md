# Examples

The examples directory includes multiple simplified well commented examples to show how the different SwitchML parts can be used in different scenarios.

## 1. Examples list

| Example | Brief |
|--|--|
| hello_world | A minimal example showing how the SwitchML client library can be used through the SwitchML context. |

## 2. Compiling

All examples require that the client library be compiled and that the SwitchML configuration file is present when running.

Also note that linking the client library code happens here.
So you usually need to provide the same build variables that you used when you compiled the client library in addition to the ones that control the example itself.
This allows the Makefile to link to the appropriate libraries for DPDK, RDMA, etc.

To compile an example, simply run (Assuming you are in the examples directory):

    make <example_name> [build variables]

Or to compile all examples at once just run 

    make [build variables]

By default, the examples executables will be found in 

    dev_root/build/bin/

### 2.1 Build variables

The following variables can all be passed to the examples makefile to control the build.

| Variable | Type | Default | Usage |
|:--:|:--:|:--:|--|
| DEBUG | boolean | 0 | Disable optimizations, add debug symbols. |
| DPDK | boolean | 0 | Add dpdk backend specific compiler/linker options. |
| MLX5 | boolean | 0 | Add dpdk backend Connect-x5/Connect-x4 specific compiler/linker options. |
| MLX4 | boolean | 0 | Add dpdk backend Connect-x3 specific compiler/linker options. |
| RDMA | boolean | 0 | Add rdma backend specific compiler/linker options. |
| BUILDDIR | path | dev_root/build | Where to store generated objects/binaries | 
| SWITCHML_HOME | path | dev_root/build | Where to look for the switchml client library installation |
| GRPC_HOME | path | dev_root/third_party/grpc/build | Where to look for the GRPC installation |
| DPDK_HOME | path | dev_root/third_party/dpdk | Where to look for the DPDK installation |
