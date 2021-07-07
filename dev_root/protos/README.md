# PROTOS

The protos directory contains the protobuf and grpc definitions that are used by both the client library and the python controller.

## 1. Compiling protos

You don't need to compile the protos yourself as that will be automatically done by the client_lib and controller makefiles. But if for whatever reason you wanted to do that then you can simply run (Assuming you are in the protos directory)

    make [build variables]

This will generate both C++ and python files.

### 1.1 Build Variables

| Variable | Type | Default | Usage |
|:--:|:--:|:--:|--|
| BUILDDIR | path | dev_root/build | Where to store generated C++ and python files | 
| GRPC_HOME | path | dev_root/third_party/grpc/build | Where to look for the GRPC installation |