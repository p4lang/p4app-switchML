
# SwitchML Client Library

The SwitchML client is a static library that bridges the gap between the end-hosts and the programmable switch through a simple to use API.

This document shows how to setup and use the library.

## 1. Backends

First of all you should know that the client library has multiple backends which perform the collective communication primitives.

### 1.1 Dummy Backend

The dummy backend is a backend that does not perform any actual communication but is just used for debugging purposes.
It helps ensure that the software stack is operating correctly down to the backend.

### 1.2 DPDK Backend

The DPDK backend uses the DPDK library to perform collective operations with the UDP transport. Thus it supports all of the NICs and drivers that DPDK supports (we tested only Intel and Mellanox NICs so far).

### 1.3 RDMA Backend

The RDMA Backend uses ibverbs directly to perform communication using RDMA as a transport and it usually outperforms DPDK on more than 10Gbps NICs because of the additional hardware offloads. However, you must have a NIC that supports RDMA.

**The full RDMA implementation is a work in progress and will be released soon.**

## 2. Required Libraries

Listed below are the system packages that are needed for the client library.

### 2.1 General requirements

These are dependencies that are required regardless of the backend you choose.

| Package (Debian/Ubuntu) | Tested Versions |
|--|--|
| gcc | 7.5.0-3ubuntu1~18.04 |
| make | 4.1-9.1ubuntu1 |
| libboost-program-options-dev | 1.65.1.0ubuntu1 |
| libgoogle-glog-dev | 0.3.5-1 |

On Debian/Ubuntu you can run the following command to install them:

	sudo apt install -y \
	gcc \
	make \
	libboost-program-options-dev \
	libgoogle-glog-dev

### 1.2 DPDK Backend Requirements

These are dependencies that are required only for the DPDK backend.

| Package (Debian/Ubuntu) | Tested Versions |
|--|--|
| libnuma-dev | 2.0.11-2.1ubuntu0.1 |
| libibverbs-dev | 46mlnx1-1.46101 |
| libmnl-dev | 1.0.4-2 |

On Debian/Ubuntu you can run the following command to install them:

	sudo apt install -y \
	libnuma-dev= \
	ibverbs= \
	mnl

In addition to the above, you must compile the dpdk library by following the instructions in the README inside the third_party directory.

### 1.3 RDMA Backend Dependencies

Coming soon!

## 2. Compiling the Library

To build the library with only the dummy backend for testing purposes you can simply run

	make

To build the library with DPDK support, add `DPDK=1` to the make command.

	make DPDK=1

By default the library will be found in:

    dev_root/build/lib/libswitchml-client.a
  
Include files will be found in

    dev_root/build/include
  
And finally the configuration file will be found in

    dev_root/build/switchml.cfg

**Notes:**
 - There are more compilation flags available that you can read about in the Makefile header.
 - A dummy backend is always compiled so even if you pass RDMA or DPDK, you can still use the dummy backend for testing and debugging.

## 3. Using the library

***Important !***
*Before trying to use the library's API directly in your project, take a look at our [Framework Integration](#) directory to see if you can simply use one of the provided methods to integrate SwitchML into your DNN software stack.*

*What follows is intended to give you a high level overview of what needs to be done. For a more detailed step by step guide look at the [examples](#)*

After building the library and getting a copy of the include files, you can now use SwitchML in your project to perform collective communication. Follow these simple steps:

1. Edit your program
	 1. Include the `context.h` file in your program.
	 3. Call `switchml::Context::GetInstance()` to retrieve the singleton instance of the Context class.
	 4. Call the `Start()` method of the context to start the SwitchML context.
	 5. Use the API provided through the context instance reference.
	 6. Call the `Stop()` method of the context to stop and cleanup the context.
 2. Compile your program
	1. Add the following to your compiler arguments
		1. `-I path_to_includes` 
		2. `-L path_to_library` 
		3.  `-l switchml_client` 
2. Configure the SwitchML clients
	1.  Before you can run your program you need to edit the configuration file that was generated after you built the library.
	2. After editing the `switchml.cfg` configuration file, copy it to where your program binary is.
4. Run your program

**Notes:**
 - You can choose to create a Config object programmatically, edit its members, and pass it to the context as a parameter of the `Start()` method, instead of using the `switchml.cfg` file.
 - For information on how to setup the switch, look at the [P4](dev_root/p4) and [controller](dev_root/controller) READMEs.
