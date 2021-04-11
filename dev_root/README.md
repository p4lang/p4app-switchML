# SwitchML Code

This directory includes all of the different SwitchML components.

```
switch_controller: controller program
client_lib: end-host library
examples: set of example programs
frameworks_integration: code to integrate with ML frameworks
third_party: third party software
```

To quickly build all components you can use the root Makefile in this directory however it is best that you go over and build each component seperately so 
that you understand the requirements needed by each component. The root Makefile exists for convenience in case you have satisfied all dependencies and want
to quickly build all of the components.

If you still want to use the root Makefile, you can simply run

    make [compilation_flags]

For example, to build the project with the DPDK backend for Mellanox ConnectX-5 we can run

    make DPDK=1 MLX5=1

To have a minimal project build that only includes the dummy backend we can run

    make

To only build the client library we can run

    make client_lib [compilation_flags]

**Notes:**
 - There are more compilation flags available that you can read about in the Makefile header.