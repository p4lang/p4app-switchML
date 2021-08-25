# SwitchML Project
# @file dpdk.dockerfile
# @brief Generates an image that can be used to run pytorch with switchml dpdk integrated.
#
# IMPORTANT: For DPDK to work, you must pass `...`
#
# A typical command to start the container is:
# ...
#

FROM nvidia/cuda:11.2.2-devel-ubuntu18.04

# Set default shell to /bin/bash
SHELL ["/bin/bash", "-cu"]

ARG TIMEOUTS=1
ARG VCL=1
ARG DEBUG=0
ARG MLX5=0
ARG MLX4=0

# General packages needed
RUN apt-get update && \
    apt install -y \
    git \
    vim \
    nano \
    wget \
    build-essential \
    net-tools \
    sudo \
    gpg \ 
    lsb-release \
    software-properties-common

# Add kitware's APT repository for cmake
RUN wget -O - https://apt.kitware.com/keys/kitware-archive-latest.asc 2>/dev/null | \
    gpg --dearmor - | \
    tee /etc/apt/trusted.gpg.d/kitware.gpg >/dev/null && \
    apt-add-repository "deb https://apt.kitware.com/ubuntu/ $(lsb_release -cs) main" && \
    apt update

# General and DPDK client library requirements
RUN apt install -y \
    gcc \
    make \
    libboost-program-options-dev \
    libgoogle-glog-dev \
    libnuma-dev \
    libibverbs-dev \
    libmnl-dev \
    autoconf \
    libtool \
    pkg-config \
    cmake \
    libssl-dev \
    linux-headers-$(uname -r) \
    linux-modules-$(uname -r)

# Clone the switchml repo and compile the client library with the benchmarks and examples.
ARG SWITCHML_UPDATED
RUN git clone --recursive https://github.com/p4lang/p4app-switchML.git /home/switchml && \
    cd /home/switchml/dev_root && \
    make DPDK=1 MLX5=${MLX5} MLX4=${MLX4} TIMEOUTS=${TIMEOUTS} VCL=${VCL} DEBUG=${DEBUG}

# At this point the microbenchmark can be run with the dummy backend.
# What follows are the neccessary steps to integrate with PyTorch.

# Install miniconda because we will need it to build and run PyTorch.
RUN cd /usr/local && \
    wget https://repo.continuum.io/miniconda/Miniconda3-latest-Linux-x86_64.sh -O ./miniconda.sh && \
    bash miniconda.sh -b -p /usr/local/conda && \
    rm miniconda.sh && \
    /usr/local/conda/bin/conda init bash
ENV PATH $PATH:/usr/local/conda/bin

# Install pytorch requirments
RUN conda env update --name base --file /home/switchml/dev_root/frameworks_integration/pytorch_patch/environment.yml

# Download PyTorch and checkout to 1.7.1 
RUN git clone https://github.com/pytorch/pytorch.git /home/pytorch && \
    cd /home/pytorch && \
    git checkout 57bffc3 && \
    git submodule sync && \
    git submodule update --init --recursive

# Make and apply the PyTorch patch
RUN cd /home/switchml/dev_root/ && \
    make pytorch_patch DPDK=1 MLX5=${MLX5} MLX4=${MLX4} && \
    git apply /home/switchml/dev_root/build/switchml_pytorch.patch

# Build PyTorch
RUN cd /home/pytorch && \
    CUDA_HOME=/usr/local/conda/ BUILD_TEST=0 /usr/local/conda/bin/python setup.py install --prefix=/usr/local/conda/ 2>&1 | tee build.log

# At this point you have the base conda environment with switchml integrated into pytorch.
# You can run training scripts and benchmarks and switchml will kick in for all reduce operations
# that it can handle. Just make sure to use the gloo pytorch backend as that's
# the pytorch backend we integrate into.