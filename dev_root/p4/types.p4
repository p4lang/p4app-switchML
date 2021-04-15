#ifndef _TYPES_
#define _TYPES_

#include "configuration.p4"

// Mirror types
typedef bit<3> mirror_type_t;

const mirror_type_t MIRROR_TYPE_I2E = 1;
const mirror_type_t MIRROR_TYPE_E2E = 2;

// Ethernet-specific types
typedef bit<48> mac_addr_t;
typedef bit<16> ether_type_t;

const ether_type_t ETHERTYPE_IPV4   = 16w0x0800;
const ether_type_t ETHERTYPE_ARP    = 16w0x0806;
const ether_type_t ETHERTYPE_ROCEv1 = 16w0x8915;

// IPv4-specific types;
typedef bit<32> ipv4_addr_t;
enum bit<8> ip_protocol_t {
    ICMP = 1,
    UDP  = 17
}

// ARP-specific types
enum bit<16> arp_opcode_t {
    REQUEST = 1,
    REPLY   = 2
}

// ICMP-specific types
enum bit<8> icmp_type_t {
    ECHO_REPLY   = 0,
    ECHO_REQUEST = 8
}

// UDP-specific types;
typedef bit<16> udp_port_t;

const udp_port_t UDP_PORT_ROCEV2        =   4791;
const udp_port_t UDP_PORT_SWITCHML_BASE = 0xbee0;
const udp_port_t UDP_PORT_SWITCHML_MASK = 0xfff0;

// IB/RoCE-specific types:
typedef bit<128> ib_gid_t;
typedef bit<24> sequence_number_t;
typedef bit<24> queue_pair_t;
typedef bit<32> rkey_t;
typedef bit<64> addr_t;

// UC opcodes
enum bit<8> ib_opcode_t {
    UC_SEND_FIRST                = 8w0b00100000,
    UC_SEND_MIDDLE               = 8w0b00100001,
    UC_SEND_LAST                 = 8w0b00100010,
    UC_SEND_LAST_IMMEDIATE       = 8w0b00100011,
    UC_SEND_ONLY                 = 8w0b00100100,
    UC_SEND_ONLY_IMMEDIATE       = 8w0b00100101,
    UC_RDMA_WRITE_FIRST          = 8w0b00100110,
    UC_RDMA_WRITE_MIDDLE         = 8w0b00100111,
    UC_RDMA_WRITE_LAST           = 8w0b00101000,
    UC_RDMA_WRITE_LAST_IMMEDIATE = 8w0b00101001,
    UC_RDMA_WRITE_ONLY           = 8w0b00101010,
    UC_RDMA_WRITE_ONLY_IMMEDIATE = 8w0b00101011
}

typedef bit<(max_num_queue_pairs_log2)> queue_pair_index_t;

// Worker types
enum bit<2> worker_type_t {
    FORWARD_ONLY = 0,
    SWITCHML_UDP = 1,
    ROCEv2       = 2
}

typedef bit<16> worker_id_t; // Same as rid for worker; used when retransmitting RDMA packets
typedef bit<32> worker_bitmap_t;
struct worker_bitmap_pair_t {
    worker_bitmap_t first;
    worker_bitmap_t second;
}

// Type to hold number of workers for a job
typedef bit<8> num_workers_t;
struct num_workers_pair_t {
    num_workers_t first;
    num_workers_t second;
}

// Type used to index into register array
typedef bit<15> pool_index_t;
typedef bit<14> pool_index_by2_t;
typedef bit<16> worker_pool_index_t;

typedef bit<32> value_t;
struct value_pair_t {
    value_t first;
    value_t second;
}

typedef bit<16> exponent_t;
struct exponent_pair_t {
    exponent_t first;
    exponent_t second;
}

// RDMA MTU (packet size). Matches ibv_mtu enum in verbs.h
enum bit<3> packet_size_t {
    IBV_MTU_128  = 0, // not actually defined in IB, but useful for no recirculation tests
    IBV_MTU_256  = 1,
    IBV_MTU_512  = 2,
    IBV_MTU_1024 = 3
}

// Drop probability between 0 and 32767
typedef bit<16> drop_probability_t;

// Type for counters
typedef bit<32> counter_t;

typedef bit<4> packet_type_underlying_t;
enum bit<4> packet_type_t {
    MIRROR     = 0x0,
    BROADCAST  = 0x1,
    RETRANSMIT = 0x2,
    IGNORE     = 0x3,
    CONSUME0   = 0x4,
    CONSUME1   = 0x5,
    CONSUME2   = 0x6,
    CONSUME3   = 0x7,
    HARVEST0   = 0x8,
    HARVEST1   = 0x9,
    HARVEST2   = 0xa,
    HARVEST3   = 0xb,
    HARVEST4   = 0xc,
    HARVEST5   = 0xd,
    HARVEST6   = 0xe,
    HARVEST7   = 0xf
}

// Port metadata, used for drop simulation
struct port_metadata_t {
    drop_probability_t ingress_drop_probability;
    drop_probability_t egress_drop_probability;
}

// SwitchML metadata header, bridged for recirculation and not exposed outside the switch
@pa_no_overlay("ingress", "ig_md.switchml_md.simulate_egress_drop")
@flexible
header switchml_md_h {

    MulticastGroupId_t mgid; // 16 bits

    queue_pair_index_t recirc_port_selector;

    packet_size_t packet_size;

    worker_type_t worker_type;
    worker_id_t worker_id;

    // Dest port or QPN to be used for responses
    bit<16> src_port;
    bit<16> dst_port;

    // What should we do with this packet?
    packet_type_t packet_type;

    // This needs to be 0xFFFF
    bit<16> ether_type_msb;

    // Index of pool elements, including both sets
    pool_index_t pool_index;

    // 0 if first packet, 1 if last packet
    num_workers_t first_last_flag;

    // 0 if packet is first packet; non-zero if retransmission
    worker_bitmap_t map_result;

    // Bitmap value before the current worker is ORed in
    worker_bitmap_t worker_bitmap_before;

    // TSI used to fill in switchML header (or RoCE address later)
    bit<32> tsi;
    bit<8> job_number;

    PortId_t ingress_port;

    // Egress drop flag
    bool simulate_egress_drop;

    // Number of workers
    num_workers_t num_workers;
}

// Bridged metadata header for RDMA
@flexible
header switchml_rdma_md_h {
    bool first_packet;
    bool last_packet;
    bit<64> rdma_addr;
}

// Metadata for ingress stage
@flexible
struct ingress_metadata_t {
    switchml_md_h switchml_md;
    switchml_rdma_md_h switchml_rdma_md;

    // This bitmap has one bit set for the current packet's worker
    worker_bitmap_t worker_bitmap;

    // Pool set, used for shadow copy
    bit<1> pool_set;

    // Checksum stuff
    bool checksum_err_ipv4;
    bool update_ipv4_checksum;

    // switch MAC and IP
    mac_addr_t switch_mac;
    ipv4_addr_t switch_ip;

    port_metadata_t port_metadata;
}

// Metadata for egress stage
struct egress_metadata_t {
    switchml_md_h switchml_md;
    switchml_rdma_md_h switchml_rdma_md;

    // Checksum stuff
    bool checksum_err_ipv4;
    bool update_ipv4_checksum;
}

#endif /* _TYPES_ */
