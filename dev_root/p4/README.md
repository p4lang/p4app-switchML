# SwitchML P4 program

The SwitchML P4 program is written in P4-16 for the [Tofino Native Architecture (TNA)](https://github.com/barefootnetworks/Open-Tofino) and the controller uses the Barefoot Runtime Interface (BRI) to program the switch.

## 1. Requirements
The P4 code has been tested on Intel P4 Studio 9.9.0.

For details on how to obtain and compile P4 Studio, we refer you to the official [Intel documentation](https://www.intel.com/content/www/us/en/products/network-io/programmable-ethernet-switch.html).

The document [[1]](#1) provides all the instructions to compile P4 Studio (aka SDE) and a P4 program. Here we show one possible way to compile the SDE using the P4 Studio build tool.

Assuming that the `SDE` environment variable points to the SDE folder, you can use the following commands to compile it for P4-16 development:

```bash
cd $SDE/p4studio
sudo -E ./install-p4studio-dependencies.sh
./p4studio profile apply ./profiles/all-tofino.yaml
```

You might also need to compile a BSP package, depending on your switch platform. The Intel documentation has the BSP package and compilation instructions for the reference platform.

The control plane requires python 3.8, so P4 Studio must be compiled using python 3, so that the generated Barefoot Runtime libraries will be compatible with python 3.

## 2. Running the P4 program

1. Build the P4 code. Detailed instructions are available in the Intel documentation [[1]](#1). Assuming that you are currently in the p4 directory, one way to compile the P4 program is with the following commands:

    ```bash
    mkdir build && cd build
    cmake $SDE/p4studio/ -DCMAKE_INSTALL_PREFIX=$SDE_INSTALL \
                         -DCMAKE_MODULE_PATH=$SDE/cmake \
                         -DP4_NAME=SwitchML \
                         -DP4_PATH=`pwd`/../switchml.p4
    make SwitchML
    make install
    ```

2. Run the reference driver application:

    ```bash
    $SDE/run_switchd.sh -p SwitchML
    ```

3. When switchd is started, run the control plane program (either on a switch or on a separate server).

## 3. Design
**This section is a work in progress**

### 3.1 SwitchML packet formats:

SwitchML currently supports two packet formats: UDP and RoCEv2.

With UDP, SwitchML packets carry a dedicated header between UDP and the payload. A range of UDP ports [0xBEE0, 0xBEEF] are used as destination/source ports in packets going received/sent by the switch. Currently we support a payload that is either 256B or 1024B (using recirculation). This is the overall packet format:

| Ethernet | IPv4 | UDP | SwitchML | Payload | Ethernet FCS |
|--|--|--|--|--|--|

<br/>

With RDMA, the packet layout is slightly different depending on which part of a message a packet contains. A message with a single packet looks like this:

| Ethernet | IPv4 | UDP | IB BTH | IB RETH | IB IMM | Payload | IB ICRC | Ethernet FCS |
|--|--|--|--|--|--|--|--|--|

<br/>

The P4 program does not check nor update the ICRC value, so the end-host servers should disable ICRC checking.

## References
<a id="1">[1]</a> Intel® P4 Studio Software Development Environment (SDE) Installation Guide
