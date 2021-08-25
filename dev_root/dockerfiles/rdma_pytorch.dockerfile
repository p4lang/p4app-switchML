# SwitchML Project
# @file rdma_pytorch.dockerfile
# @brief Generates an image that can be used to run pytorch with switchml rdma integrated.
#
# IMPORTANT: For RDMA to work, you must pass `--cap-add=IPC_LOCK --device=/dev/infiniband/<NIC name>` arguments when starting
# You also still need to manually disable ICRC checking on the host machine. Take a look at our disable_icrc.sh script in the scripts folder.
#
# A typical command to start the container is:
# docker run -it --gpus all --net=host --cap-add=IPC_LOCK --device=/dev/infiniband/uverbs0 --name switchml-rdma-pytorch <name_of_the_image_created_from_this_file>
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

# Install miniconda
RUN cd /usr/local && \
    wget https://repo.continuum.io/miniconda/Miniconda3-latest-Linux-x86_64.sh -O ./miniconda.sh && \
    bash miniconda.sh -b -p /usr/local/conda && \
    rm miniconda.sh && \
    /usr/local/conda/bin/conda init bash && \
    /usr/local/conda/bin/conda install -y numpy jupyter matplotlib
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
    make pytorch_patch RDMA=1 && \
    git apply /home/switchml/dev_root/build/switchml_pytorch.patch

# Build PyTorch
RUN cd /home/pytorch && \
    CUDA_HOME=/usr/local/conda/ BUILD_TEST=0 /usr/local/conda/bin/python setup.py install --prefix=/usr/local/conda/ 2>&1 | tee build.log

# At this point you have the base conda environment with switchml integrated into pytorch.
# You can run training scripts and benchmarks and switchml will kick in for all reduce operations
# that it can handle. Just make sure to use the gloo pytorch backend as that's
# the pytorch backend we integrate into.