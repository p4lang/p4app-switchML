# SwitchML Project
# @file controller.dockerfile
# @brief Generates an image that can be used to launch the switchml controller.
#
# IMPORTANT: For the controller to work, it must have access to python modules in the Barefoot SDE installation directory.
# Therefore you should mount the SDE installation directory when you start the container as a shared directory that exists in /home/bf-sde-install in the container.
# 
# A typical command to start the container is:
# docker run -it -v $SDE_INSTALL:/home/bf-sde-install --net=host --name switchml-controller <name_of_the_image_created_from_this_file>
#

FROM ubuntu:18.04

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

# Python controller requirements
RUN apt install -y \
    make \
    autoconf \
    libtool \
    pkg-config \
    cmake \
    libssl-dev

# Install miniconda to manage python versions and packages easily
RUN cd /usr/local && \
    wget https://repo.continuum.io/miniconda/Miniconda3-latest-Linux-x86_64.sh -O ./miniconda.sh && \
    bash miniconda.sh -b -p /usr/local/conda && \
    rm miniconda.sh && \
    /usr/local/conda/bin/conda init bash && \
    /usr/local/conda/bin/conda install -y numpy jupyter matplotlib
ENV PATH $PATH:/usr/local/conda/bin

RUN pip3 install \
    pyyaml \
    grpcio>=1.34.0 \
    protobuf \
    asyncio \
    ipaddress \
    ansicolors \
    google-api-python-client

# Clone the switchml repo and compile grpc and the protobufs needed for the controller
ARG SWITCHML_UPDATED
RUN git clone https://github.com/p4lang/p4app-switchML.git /home/switchml && \
    cd /home/switchml/dev_root && \
    git submodule update --init --recursive -- third_party/grpc && \
    cd third_party && \
    make grpc

# Register the compiled GRPC
# You can skip this step however you would need to use the LD_LIBRARY_PATH variable each time you run 
# any application with switchml
RUN echo /home/switchml/dev_root/third_party/grpc/build/lib > /etc/ld.so.conf.d/000_grpc.conf && \
    ldconfig

RUN cd /home/switchml/dev_root/controller && \
    make

ENV SDE_INSTALL /home/bf-sde-install