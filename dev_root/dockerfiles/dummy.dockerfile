# SwitchML Project
# @file dummy.dockerfile
# @brief Generates an image that can be used to run switchml's benchmarks with the dummy backend.
#
# A typical command to start the container is:
# docker run -it --gpus all --net=host --name switchml-dummy <name_of_the_image_created_from_this_file>
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
    sudo

# General client library requirements
RUN apt install -y \
    gcc \
    make \
    libboost-program-options-dev \
    libgoogle-glog-dev

# Clone the switchml repo and compile the client library with the benchmarks and examples.
ARG SWITCHML_UPDATED
RUN git clone https://github.com/p4lang/p4app-switchML.git /home/switchml && \
    cd /home/switchml/dev_root && \
    git submodule update --init -- third_party/vcl
    make TIMEOUTS=${TIMEOUTS} VCL=${VCL} DEBUG=${DEBUG}

# At this point the microbenchmark can be run with the dummy backend.
