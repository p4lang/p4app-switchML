# SwitchML Code

This directory includes all of the different SwitchML components.

```
switch_p4: SwitchML P4 code for TNA
switch_controller: controller program
client_lib: end-host library
examples: set of example programs
benchmarks: programs used to test raw performance
frameworks_integration: code to integrate with ML frameworks
third_party: third party software
```

## 1. Switch
We provide the [P4 program](dev_root/switch_p4) for the [Tofino Native Architecture (TNA)](https://github.com/barefootnetworks/Open-Tofino) and the [runtime python controller](dev_root/switch_controller). We assume that you have available and have already compiled the Intel P4 Studio suite in both the switch and the server that will run the controller. You can also run the controller in the switch itself.
For details on how to obtain and compile Intel P4 Studio, we refer you to the official [Intel documentation](https://www.intel.com/content/www/us/en/products/network-io/programmable-ethernet-switch.html).

## 2. Library
To quickly build all end-host components (library, examples and benchmarks) you can use the root Makefile in this directory, however we suggest that you go over and build each component seperately so that you understand the requirements needed by each component. The root Makefile exists for convenience in case you have satisfied all dependencies and want to quickly build all of the components.

If you still want to use the root Makefile, you can simply run:

    make [compilation_flags]

For example, to build the project with the DPDK backend for Mellanox ConnectX-5 you can run:

    make DPDK=1 MLX5=1

To have a minimal project build that only includes the dummy backend you can run:

    make

To only build the client library you can run:

    make client_lib [compilation_flags]

**Notes:**
 - There are more compilation flags available that you can read about in the Makefile header.
