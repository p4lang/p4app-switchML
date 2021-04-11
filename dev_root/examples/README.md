# Examples

This directory includes multiple simplified well commented examples to show how the different SwitchML parts can be used in different scenarios.

## Examples list

| Example | Brief | Compilation flags |
|--|--|--|
| hello_world | A minimal example showing how the SwitchML client library can be used through the SwitchML context. | N/A |

## Compiling

All examples require that the client library be compiled and that the switchml configuration file is present when running.

Also note that linking the client library code happens here..
So you usually need to provide the same compilation flags that you used when you compiled the client library in addition to the ones that control the example itself.
This allows the Makefile to link to the appropriate libraries for dpdk, rdma..etc.

To compile an example, simply run:

    make <example_name> [compilation flags]

Or to compile all examples at once just run 

    make [compilation flags]

By default, the examples executables will be found in 

    dev_root/build/bin/