# Pytorch Patch

By changing a few lines of code in PyTorch we are able to delegate allreduce SUM operations to switchml.

We take advantage of PyTorch's gloo backend and customize it so that it uses switchml instead of Gloo for operations and data types that switchml supports.
If a job is not supported by switchml then pytorch automatically fallsback to using gloo.

For switchml to take over from Gloo the following conditions must be met:
- The all reduce operation must be a summation
- The data type must be float or int32
- Each node/host produces 1 tensor or in other words each node/host uses 1 GPU.

## 1. Building pytorch

#### 1. Creating the CONDA environment

    conda env create --prefix path_to_env --file environment.yml

This will take a good while to install all of the required libraries.

#### 2. Download PyTorch

The PyTorch patch applies to a specific commit which we must checkout to.

    git clone https://github.com/pytorch/pytorch.git
    cd pytorch
    git checkout 57bffc3 # The 1.7.1 version
    git submodule sync
    git submodule update --init --recursive 

This will also take a good while to clone and checkout all submodules.

#### 3. Apply the switchml pytorch patch to pytorch

First, you need to generate the patch as the switchml_pytorch.patch in the pytorch_patch directory is not ready.
Run make with the same build variables that you used to compile the library.
This is important as pytorch needs to know about the backend specific libraries that it needs to link to.

    cd repo_home/dev_root/frameworks_integration/pytorch_patch
    make <variable flags>

Then 

    cd path_to_cloned_pytorch
    git apply path_to_switchml_build_dir/switchml_pytorch.patch

Keep in mind that the patch will hard code the local default absolute paths of the switchml library, grpc, and dpdk (in case you are using the dpdk backend)
into the pytorch code.
Thus, if you change the location of these libraries you should generate and apply another patch after passing the `SWITCHML_HOME`, `DPDK_HOME`, `GRPC_HOME` 
variables correctly to the pytorch patch makefile.

All build variables that can be passed:

| Variable | Type | Default | Usage |
|:--:|:--:|:--:|--|
| DEBUG | boolean | 0 | Disable optimizations, add debug symbols, and enable detailed debugging messages. |
| DPDK | boolean | 0 | Add dpdk backend specific compiler/linker options. |
| MLX5 | boolean | 0 | Add dpdk backend Connect-x5/Connect-x4 specific compiler/linker options. |
| MLX4 | boolean | 0 | Add dpdk backend Connect-x3 specific compiler/linker options. |
| RDMA | boolean | 0 | Add rdma backend specific compiler/linker options. |
| BUILDDIR | path | dev_root/build | Where to store the generated pytorch patch. | 
| SWITCHML_HOME | path | dev_root/build/ | Where to look for the switchml client library installation. |
| GRPC_HOME | path | dev_root/third_party/grpc/build | Where to look for the GRPC installation |
| DPDK_HOME | path | dev_root/third_party/dpdk/build |  Where to look for the DPDK installation |

#### 4. Build PyTorch

PyTorch has lots of compilation flags and options. Advanced users should check the official repository's documentation to learn how to tailor the resulting library to their specific needs.

Activate your conda environment

    conda activate conda_env_prefix

Build PyTorch

    CUDA_HOME=${CONDA_PREFIX} BUILD_TEST=0 USE_STATIC_NCCL=1 ${CONDA_PREFIX}/bin/python setup.py install --prefix=${CONDA_PREFIX} 2>&1 | tee build.log

This will again take a good while.
The script should run to completion without errors (Warnings are probably fine).
Once that's done you would have a conda environment that has SwitchML integrated into the PyTorch library.

You can then simply run your normal pytorch distributed training scripts.
The only thing that you **must** do is choose **gloo** as the backend and SwitchML will kick in automatically when it can handle the collective operation.

You may be interested in installing torchvision as well by running

    pip install torchvision==0.8.2 pillow --no-dependencies

We pass no-dependencies because pip may attempt to install another version of pytorch and uninstall our patched one.