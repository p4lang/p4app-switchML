# NCCL Plugin

**IMPORTANT: This method currently has performance issues. Use PyTorch patch instead**.

NCCL provides a way to override its collective communication functions through the use of plugins.
Any NCCL installation looks for a libnccl-net.so library (The library is the plugin) before it starts. If its available then it loads it at runtime and uses it. This allows us to seemlessly integrate with any framework that uses NCCL.

Currently tested frameworks:
    - PyTorch
    - Tensorflow through Horovod

## 1. Building NCCL
First thing that we need to do is modify NCCL to allow our plugin to only implement the functions that are relevent to SwitchML and fallback to existing algorithms for the others.

We are working towards avoiding the need to rebuild a modified NCCL but it is required at the moment. 

1. Make sure CUDA is installed. 

2. Clone the NCCL repository

    git clone https://github.com/NVIDIA/nccl.git

3. Checkout to the specific version that we modified

    cd nccl
    git checkout 195232

4. Apply the NCCL patch `nccl_collectives.patch`

    git apply p4app-switchml/frameworks_integration/nccl_plugin/nccl_collectives.patch

5. Compile the patched NCCL (If CUDA is not installed in the default path then provide its path using CUDA=<path>)

    make -j src.build

6. Register our custom NCCL in the system.

This step is required unless you have installed  the patched NCCL directly on the system.

Run:

    sudo echo path_to_patched_nccl_repo/build/lib >> /etc/ld.so.conf.d/1switchml.conf
    sudo ldconfig

To remove this registration so that you can use your normal version of NCCL simply undo what you did

    sudo rm /etc/ld.so.conf.d/1switchml.conf
    sudo ldconfig

## 2. Building the plugin

1. Make sure that the client library is built and ready. Refer to the client_lib folder for details on how to build it.

2. Build the nccl plugin.

From the nccl_plugin directory run

    make NCCL_HOME=<path_to_patched_nccl_repo/build/> CUDA_HOME=<path to cuda installation>

3. Register the nccl plugin

At this point you should confirm that you have a libnccl-net.so shared library in the build directory.
This means that the nccl plugin is ready. But now NCCL must be aware that this library/plugin exists.

Run:

    sudo echo "path_to_switchml_repo/build/lib" >> /etc/ld.so.conf.d/1switchml.conf
    sudo ldconfig

## 3. Forcing NCCL to use the plugin
Now we just need to set some environement variables to force NCCL to use our plugin

    sudo echo NCCL_COLLNET_ENABLE=1 > /etc/nccl.conf
    sudo echo NCCL_ALGO=CollNet > /etc/nccl.conf
    sudo echo NCCL_CHECKS_DISABLE=1 >> /etc/nccl.conf # Can improve performance
    sudo echo NCCL_IB_DISABLE=1 >> /etc/nccl.conf

You can always skip this and set these environment variables in your scripts or conda environments instead.

## 4. NCCL tests (Optional)

Now to verify that everything is working up to this point.
We can build and run the NCCL tests.

1. Make sure you have a working MPI installation.

2. Clone the NCCL tests repo

    git clone https://github.com/NVIDIA/nccl-tests.git

3. Build NCCL tests

    cd nccl-tests
    make MPI=1 NCCL_HOME=path_to_patched_nccl_repo/build/ MPI_HOME=path_to_mpi_installation

4. Setup switchml configuration in the nccl-tests/build directory

5. Test allreduce

    cd build
    mpirun -np <num_processes> -host=<list_of_host_ips> ./all_reduce_perf --op sum --datatype float --iters 10 --warmup_iters 5

Useful NCCL variables for debugging `NCCL_DEBUG=INFO`, `NCCL_DEBUG_SUBSYS=ALL`, and `NCCL_CHECKS_DISABLE=0`
    
## 5. Running with PyTorch

The important thing to keep in mind when running with PyTorch is that you want PyTorch to use our patched NCCL and not their own NCCL module to which they link statically.
So depending on where you got your PyTorch binary from or how you compiled it, you may need to recompile PyTorch so that it links dynamically or statically to our patched NCCL.

## 6. Running with Tensorflow Horovod

**There are some linking problems at the moment. Instructions will come soon.**

