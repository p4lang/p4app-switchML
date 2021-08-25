# SwitchML Project
# @file rdma.dockerfile
# @brief Generates an image that can be used to run switchml's benchmarks with the rdma backend.
#
# IMPORTANT: For RDMA to work, you must pass `--cap-add=IPC_LOCK --device=/dev/infiniband/<NIC name>` arguments when starting.
# You also still need to manually disable ICRC checking on the host machine. Take a look at our disable_icrc.sh script in the scripts folder.
#
# A typical command to start the container is:
# docker run -it --gpus all --net=host --cap-add=IPC_LOCK --device=/dev/infiniband/uverbs0 --name switchml-rdma <name_of_the_image_created_from_this_file>
#

FROM nvidia/cuda:11.2.2-devel-ubuntu18.04

# Set default shell to /bin/bash
SHELL ["/bin/bash", "-cu"]

ARG TIMEOUTS=1
ARG VCL=1
ARG DEBUG=0

# General packages needed
RUN apt-get update && \
    apt install -y \
    git \
    wget \
    vim \
    nano \
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

# General and RDMA client library requirements
RUN apt install -y \
    gcc \
    make \
    libboost-program-options-dev \
    libgoogle-glog-dev \
    autoconf \
    libtool \
    pkg-config \
    libibverbs-dev \
    libhugetlbfs-dev \
    cmake \
    libssl-dev

# Clone the switchml repo and compile the client library with the benchmarks and examples.
ARG SWITCHML_UPDATED
RUN git clone https://github.com/p4lang/p4app-switchML.git /home/switchml && \
    cd /home/switchml/dev_root && \
    git submodule update --init --recursive -- third_party/vcl && \
    git submodule update --init --recursive -- third_party/grpc && \
    make RDMA=1 TIMEOUTS=${TIMEOUTS} VCL=${VCL} DEBUG=${DEBUG}

# Register the compiled GRPC
# You can skip this step however you would need to use the LD_LIBRARY_PATH variable each time you run 
# any application with switchml
RUN echo /home/switchml/dev_root/third_party/grpc/build/lib > /etc/ld.so.conf.d/000_grpc.conf && \
    ldconfig

# At this point the microbenchmark can be run with the rdma backend.