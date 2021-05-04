# Third Party

The third_party directory contains third party libraries that are needed by SwitchML.

To build a library, simply run (Assuming you are in the third_party):

    make <library name> [compilation flags]

For example, to build DPDK for Mellanox ConnectX-5

    make DPDK MLX5=1

**Notes:**
 - There are more compilation flags available that you can read about in the Makefile header.
