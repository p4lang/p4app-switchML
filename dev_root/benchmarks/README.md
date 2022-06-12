# Benchmarks

The benchmarks directory includes multiple benchmarks to test and measure the performance of the different components of SwitchML or of the system as a whole.

The benchmarks should be the go-to tool for ensuring that the performance and the accuracy of SwitchML remains as expected after any change.

## 1. Benchmarks list

| Benchmark | Brief |
|--|--|
| allreduce_benchmark | A c++ microbenchmark that tests the whole switchml stack by directly submitting multiple all-reduce jobs to SwitchML. |
| dnn_benchmark | A c++ benchmark that imitates DNN training performing real allreduce communication but sleeping to simulate compute. This benchmark requires an extra CSV file that describes the structure of the DNN in terms of the size and backward and forward pass costs (In nanoseconds) for each layer in the DNN. It goes through the forward pass of each layer (sleeping) then launches an async allreduce job after each layer backward pass. It only synchronizes with an allreduce job if its result is needed for a layer in the forward pass of the next iteration. |
| allreduce_pytorch_benchmark | A python microbenchmark that tests the whole switchml stack *through pytorch* by submitting multiple all-reduce jobs to SwitchML. This benchmark can also be used to test all different distributed backends from pytorch to serve as baselines. |

All c++ benchmarks require the compilation of the client library and that the SwitchML configuration file be present when running.

## 2. Compiling (C++ benchmarks)

To compile a benchmark, simply run (Assuming you are inside the benchmarks directory):

    make <benchmark_name> [build variables]

Or to compile all benchmarks at once just run 

    make [build variables]

By default, the benchmark executables will be found in 

    dev_root/build/bin/

### 2.1 Build variables (C++ benchmarks)

The following variables can all be passed to the benchmarks makefile to control the build.

| Variable | Type | Default | Usage |
|:--:|:--:|:--:|--|
| DEBUG | boolean | 0 | Disable optimizations, add debug symbols. |
| DPDK | boolean | 0 | Add dpdk backend specific compiler/linker options. |
| MLX5 | boolean | 0 | Add dpdk backend Connect-x5/Connect-x4 specific compiler/linker options. |
| MLX4 | boolean | 0 | Add dpdk backend Connect-x3 specific compiler/linker options. |
| RDMA | boolean | 0 | Add rdma backend specific compiler/linker options. |
| CUDA | boolean | 0 | Compile benchmark with cuda support. Allows allocating tensors on the gpu and passing their pointers to the client library. (Do not use as GPU memory is not yet handled by any of the prepostprocessors in the client library) |
| BUILDDIR | path | dev_root/build | Where to store generated objects/include files/libraries/binaries...etc. | 
| SWITCHML_HOME | path | dev_root/build | Where to look for the switchml client library installation |
| GRPC_HOME | path | dev_root/third_party/grpc/build | Where to look for the GRPC installation |
| DPDK_HOME | path | dev_root/third_party/dpdk | Where to look for the DPDK installation |
