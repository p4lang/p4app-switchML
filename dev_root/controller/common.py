import re
import socket
from enum import IntEnum

# From configuration.p4
max_num_queue_pairs_per_worker = 512

# Regexes
mac_address_regex = re.compile(':'.join(['[0-9a-fA-F]{2}'] * 6))
front_panel_regex = re.compile('([0-9]+)/([0-9]+)$')


# IPv4 validation
def validate_ip(value):
    ''' Validate IP address string '''
    try:
        socket.inet_aton(value)
        return True
    except:
        return False


# Enums
class WorkerType(IntEnum):
    FORWARD_ONLY = 0
    SWITCHML_UDP = 1
    ROCEv2 = 2


class RDMAOpcode(IntEnum):
    UC_SEND_FIRST = 0b00100000
    UC_SEND_MIDDLE = 0b00100001
    UC_SEND_LAST = 0b00100010
    UC_SEND_LAST_IMMEDIATE = 0b00100011
    UC_SEND_ONLY = 0b00100100
    UC_SEND_ONLY_IMMEDIATE = 0b00100101
    UC_RDMA_WRITE_FIRST = 0b00100110
    UC_RDMA_WRITE_MIDDLE = 0b00100111
    UC_RDMA_WRITE_LAST = 0b00101000
    UC_RDMA_WRITE_LAST_IMMEDIATE = 0b00101001
    UC_RDMA_WRITE_ONLY = 0b00101010
    UC_RDMA_WRITE_ONLY_IMMEDIATE = 0b00101011


class PacketSize(IntEnum):
    MTU_128 = 0
    MTU_256 = 1
    MTU_512 = 2
    MTU_1024 = 3


class PacketType(IntEnum):
    MIRROR = 0x0
    BROADCAST = 0x1
    RETRANSMIT = 0x2
    IGNORE = 0x3
    CONSUME0 = 0x4
    CONSUME1 = 0x5
    CONSUME2 = 0x6
    CONSUME3 = 0x7
    HARVEST0 = 0x8
    HARVEST1 = 0x9
    HARVEST2 = 0xa
    HARVEST3 = 0xb
    HARVEST4 = 0xc
    HARVEST5 = 0xd
    HARVEST6 = 0xe
    HARVEST7 = 0xf
