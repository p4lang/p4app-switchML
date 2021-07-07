# Third Party

The third_party directory contains third party libraries that are needed by SwitchML.

## 1. Building

To build a library, simply run (Assuming you are in the third_party directory):

    make <library name> [build variables]

For example, to build DPDK for Mellanox ConnectX-5

    make dpdk MLX5=1

The default target automatically builds the needed libraries based on your build variables.
For example running:

    make RDMA=1

will build grpc only since its the only library needed for the RDMA backend.
Whilst running:

    make DPDK=1 MLX5=1

will build dpdk as well as grpc since both are needed by the DPDK backend.

No building is needed for the VCL library so its ignored.

### 1.1 Build Variables

The following variables can all be passed to the third_party makefile to control which and how third party libraries are built.

| Variable | Type | Default | Usage |
|:--:|:--:|:--:|--|
| DEBUG | boolean | 0 | Disable optimizations, add debug symbols, and enable detailed debugging messages. |
| DPDK | boolean | 0 | Compile all third party libraries needed by the DPDK backend. |
| MLX5 | boolean | 0 | Configure the DPDK library for Mellanox ConnectX-4/ConnectX-5 before building |
| MLX4 | boolean | 0 | Configure the DPDK library for Mellanox ConnectX-3 before building |
| RDMA | boolean | 0 | Compile all third party libraries needed by the RDMA backend. |
