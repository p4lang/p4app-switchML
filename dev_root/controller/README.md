# Switch Controller

The SwitchML controller will program the switch at runtime using the Barefoot Runtime Interface (BRI). The controller accepts connections from end-hosts through gRPC to set up a job (which is a sequence of allreduce operations involving the same set of workers). It also provides a CLI interface that can be used to configure the switch and read counters values at runtime.

## Requirements
The controller requires python 3.8 and the following python packages:

```bash
grpcio pyyaml asyncio ipaddress
```

Additionally, the following two modules are required:

```bash
bfrt_grpc.bfruntime_pb2 bfrt_grpc.client
```

These modules are autogenerated when P4 Studio is compiled. The controller expects that the `SDE_INSTALL` environment variable points to the SDE install directory. It will search for those modules in the following folder:

```bash
$SDE_INSTALL/lib/python*/site-packages/tofino/bfrt_grpc/
```

## Running the controller

To enable switch ports and configure the switch to forward regular traffic, the controller reads the `fib.yml` that describes the machines connected to the switch ports: port number, MAC and IP addresses of the machine connected to that port and port speed.
This is an example:

```yaml
switch:
    forward:
        0   : {mac: "01:23:45:67:89:ab", ip: "10.0.0.101", speed: "100G"}
        0   : {mac: "01:23:45:ba:98:76", ip: "10.0.0.102", speed: "100G"}
```

The controller is started with:

```bash
python switchml.py
```

The optional arguments are the following:

| Argument | Description | Default |
|-|-|-|
| --bfrt_ip ADDRESS | Name/address of the BFRuntime server | 127.0.0.1 |
| --bfrt_port PORT | Port of the BF Runtime server | 50052 |
| --ports FILE | YAML file describing machines connected to ports | fib.yml |
| --program PROGRAM | P4 program name | SwitchML |
| --switch_mac SWITCH_MAC | MAC address of the switch | 01:23:45:67:89:AB |
| --switch_ip SWITCH_IP | IP address of switch | 10.0.0.254 |

The BFRuntime server is the switch reference drivers application. The switch MAC and IP are the addresses of your choosing that will be used by the switch when acting as a SwitchML endpoint.