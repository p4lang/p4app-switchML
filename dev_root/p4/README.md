# Switch P4

The SwitchML P4 program is written in P4-16 for the [Tofino Native Architecture (TNA)](https://github.com/barefootnetworks/Open-Tofino) and the controller uses the Barefoot Runtime Interface (BRI) to program the switch.

## 1. Requirements
The p4 code has been tested on Intel P4 Studio 9.2.0.

For details on how to obtain and compile P4 Studio, we refer you to the official [Intel documentation](https://www.intel.com/content/www/us/en/products/network-io/programmable-ethernet-switch.html). The P4 Studio README file provides all the instructions to compile the SDE. Here we show one possible way to compile the SDE using the P4 Studio Build tool.

Assuming that the `SDE` environment variable points to the P4 Studio folder, you can use the following commands to compile it for P4-16 development:

```bash
cd $SDE/p4studio_build
./p4studio_build.py --use-profile p416_examples_profile
```

The control plane requires python 3.8, so P4 Studio must be compiled using python 3, so that the generated Barefoot Runtime libraries will be compatible with python 3.

## 2. Running the P4 program

1. Build the P4 code. Detailed instructions are available in the Intel documentation (_Compiler User Guide_ document). If you use the `p4_build.sh` script, you can compile the P4 program with the following command:

    ```bash
    P4_NAME=SwitchML p4_build.sh switchml.p4
    ```

2. Run the reference drivers application:

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
