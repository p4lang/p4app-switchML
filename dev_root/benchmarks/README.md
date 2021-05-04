# Benchmarks

The benchmarks directory includes multiple benchmarks to test and measure the performance of the different components of SwitchML or of the system as a whole.

The benchmarks should be the go-to tool that ensures that the performance and the accuracy of SwitchML remains as expected after any change.

## 1. Benchmarks list

| Example | Brief | Compilation flags |
|--|--|--|
| allreduce_benchmark | The most complete benchmark, as it actually performs allreduce jobs thus testing the whole system. | DEBUG DPDK MLX5 MLX4 |

All examples require that the client library is compiled and that the SwitchML configuration file is present when running.

## 2. Compiling

To compile a benchmark, simply run (Assuming you are inside the benchmarks directory):

    make <benchmark_name> [compilation flags]

Or to compile all benchmarks at once just run 

    make [compilation flags]

By default, the benchmark executables will be found in 

    dev_root/build/bin/
