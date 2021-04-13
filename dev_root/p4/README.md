# Switch P4

The SwitchML P4 program is written in P4_16 for the [Tofino Native Architecture (TNA)](https://github.com/barefootnetworks/Open-Tofino) and the controller uses the Barefoot Runtime Interface (BRI) to program the switch.

## Requirements
The p4 code requires Intel P4 Studio 9.2.0 or above.

For details on how to obtain and compile P4 Studio, we refer you to the official [Intel documentation](https://www.intel.com/content/www/us/en/products/network-io/programmable-ethernet-switch.html).

The control plane requires python 3.8, so P4 Studio must be compiled using python 3, so that the generated Barefoot Runtime libraries will be compatible with python 3.

## Running the P4 program

1. Build the P4 code using the `p4_build.sh` script like this (follow the Intel documentation for detailed instructions):

    ```bash
    P4_NAME=SwitchML ./p4_build.sh  switchml.p4
    ```

2. Run the reference drivers application:

    ```bash
    ./run_switchd.sh  -p SwitchML
    ```

3. When switchd is started, run the control plane program (either on a switch or on a separate server).

## Design
**This section is a work in progress**

- SwitchML packet formats:

    SwitchML currently supports two packet formats: UDP and RoCEv2.

    With UDP, SwitchML packets carry a dedicated header between UDP and the payload. A range of UDP ports [0xBEE0, 0xBEEF] are used as destination/source ports in packets going received/sent by the switch. Currently we support a payload that is either 256B or 1024B (using recirculation). This is the overall packet format: 

    <table>
      <tbody>
        <tr>
          <td>Ethernet</td>
          <td>IPv4</td>
          <td>UDP</td>
          <td>SwitchML</td>
          <td>Payload</td>
          <td>Ethernet FCS</td>
        </tr>
      </tbody>
    </table>
    <br/>

    With RDMA, the packet layout is slightly different depending on which part of a message a packet contains. A message with a single packet looks like this:

    <table>
      <tbody>
        <tr>
          <td>Ethernet</td>
          <td>IPv4</td>
          <td>UDP</td>
          <td>IB BTH</td>
          <td>IB RETH</td>
          <td>IB IMM</td>
          <td>Payload</td>
          <td>IB ICRC</td>
          <td>Ethernet FCS</td>
        </tr>
      </tbody>
    </table>
    <br/>

    The P4 program does not check nor update the ICRC value, so the end-host servers should disable ICRC checking.
