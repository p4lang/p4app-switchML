# SwitchML Project
# @file dpdk.dockerfile
# @brief Generates an image that can be used to run switchml's benchmarks with the dpdk backend.
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