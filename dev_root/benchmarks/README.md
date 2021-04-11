# Benchmarks

This directory includes multiple benchmarks to test and measure the performance of the different components of SwitchML
or of the system as a whole. 

The benchmarks should be the go to tool that ensures that the performance and the accuracy of SwitchML remains as expected after any change.

## Benchmarks list

| Example | Brief | Compilation flags |
|--|--|--|
| allreduce_benchmark | The most wholistic benchmark as it actually performs allreduce jobs thus testing the whole system. | DEBUG DPDK MLX5 MLX4 |

All examples require that the client library be compiled and that the switchml configuration file is present when running.

## Compiling

To compile a benchmark, simply run:

    make <benchmark_name> [compilation flags]

Or to compile all benchmarks at once just run 

    make [compilation flags]

By default, the benchmark executables will be found in 

    dev_root/build/bin/