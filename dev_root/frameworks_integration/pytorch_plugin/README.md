# Pytorch Plugin

A complete SwitchML pytorch backend that can be installed as a plugin without the need to patch PyTorch or build PyTorch from source.

This plugin is meant to eventually replace the PyTorch patch integration method for the following reasons:

 1. A clean PyTorch backend created for SwitchML only.
 2. Eliminates the need to patch PyTorch and compile from source
 3. Requires 10 seconds to install in an existing PyTorch environment as opposed to 30-60 minutes of compiling the whole PyTorch framework and going through the pains associated with that.

**Disclaimer** It is important to note that the plugin is still in the early stages of development and has not been thoroughly tested for correctness and performance. Currently it only supports summation allreduce operations for Int32 and Floats.

## Installing the plugin
### 1. Creating the CONDA environment (Optional)
If you already have a working conda environment with PyTorch installed, you can skip this part.
The plugin was tested with PyTorch 1.11 and 1.12 but should work with previous versions as long as they support distributed cpp plugins.

We have provided a conda environment.yml file which contains all needed dependencies for Linux. You can create it by running the following (Assuming you are inside the pytorch_plugin directory)

    conda env create --prefix path_to_env --file environment.yml

This will take a good while to install all of the required libraries.

### 2. Building and installing the plugin
Assuming you already inside the PyTorch environment that you want to install SwitchML in, and inside the pytorch_plugin directory. You can simply run the following:

    [switchml build variables] python setup.py install

Run with the same build variables that you used to compile the switchml client library. This shouldn't take more than a few seconds.

The build should finish without any errors and you should now see that the `torch_switchml` package has been installed in your environment.
    
#### 2.1 Build variables
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
| DPDK_HOME | path | dev_root/third_party/dpdk/build | Where to look for the DPDK installation |

## Using the plugin

To use SwitchML within PyTorch, you need to do the following in your pytorch distributed script:

 1. `import torch_switchml`
 2. When initializing the PyTorch distributed backend, put `sml` as your backend.
`dist.init_process_group(backend="sml", ....`
3. Unrelated to your script but do not forget to include a switchml configuration file within your current directory !**

That's it, you can now enjoy the SwitchML speedups within PyTorch !



