/*
  Copyright 2021 Intel-KAUST-Microsoft

  Licensed under the Apache License, Version 2.0 (the "License");
  you may not use this file except in compliance with the License.
  You may obtain a copy of the License at

     http://www.apache.org/licenses/LICENSE-2.0

  Unless required by applicable law or agreed to in writing, software
  distributed under the License is distributed on an "AS IS" BASIS,
  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
  See the License for the specific language governing permissions and
  limitations under the License.
*/

#ifndef _HEADERS_
#define _HEADERS_

#include "types.p4"

header ethernet_h {
    mac_addr_t dst_addr;
    mac_addr_t src_addr;
    bit<16>    ether_type;
}

header ipv4_h {
    bit<4>        version;
    bit<4>        ihl;
    bit<8>        diffserv;
    bit<16>       total_len;
    bit<16>       identification;
    bit<3>        flags;
    bit<13>       frag_offset;
    bit<8>        ttl;
    ip_protocol_t protocol;
    bit<16>       hdr_checksum;
    ipv4_addr_t   src_addr;
    ipv4_addr_t   dst_addr;
}

header icmp_h {
    icmp_type_t msg_type;
    bit<8>      msg_code;
    bit<16>     checksum;
}

header arp_h {
    bit<16>       hw_type;
    ether_type_t  proto_type;
    bit<8>        hw_addr_len;
    bit<8>        proto_addr_len;
    arp_opcode_t  opcode;
}

header arp_ipv4_h {
    mac_addr_t   src_hw_addr;
    ipv4_addr_t  src_proto_addr;
    mac_addr_t   dst_hw_addr;
    ipv4_addr_t  dst_proto_addr;
}

header udp_h {
    bit<16> src_port;
    bit<16> dst_port;
    bit<16> length;
    bit<16> checksum;
}

// SwitchML header
header switchml_h {
    bit<4> msg_type;
    bit<1> unused;
    packet_size_t size;
    bit<8> job_number;
    bit<32> tsi;
    bit<16> pool_index;
}

// InfiniBand-RoCE Base Transport Header
header ib_bth_h {
    ib_opcode_t       opcode;
    bit<1>            se;
    bit<1>            migration_req;
    bit<2>            pad_count;
    bit<4>            transport_version;
    bit<16>           partition_key;
    bit<1>            f_res1;
    bit<1>            b_res1;
    bit<6>            reserved;
    queue_pair_t      dst_qp;
    bit<1>            ack_req;
    bit<7>            reserved2;
    sequence_number_t psn;
}

@pa_container_size("ingress", "hdr.ib_bth.psn", 32)

// InfiniBand-RoCE RDMA Extended Transport Header
header ib_reth_h {
    bit<64> addr;
    bit<32> r_key;
    bit<32> len;
}

// InfiniBand-RoCE Immediate Header
header ib_immediate_h {
    bit<32> immediate;
}

// InfiniBand-RoCE ICRC Header
header ib_icrc_h {
    bit<32> icrc;
}

// 2-byte exponent header (assuming exponent_t is bit<16>)
header exponents_h {
    exponent_t e0;
}

// 128-byte data header
header data_h {
    value_t d00;
    value_t d01;
    value_t d02;
    value_t d03;
    value_t d04;
    value_t d05;
    value_t d06;
    value_t d07;
    value_t d08;
    value_t d09;
    value_t d10;
    value_t d11;
    value_t d12;
    value_t d13;
    value_t d14;
    value_t d15;
    value_t d16;
    value_t d17;
    value_t d18;
    value_t d19;
    value_t d20;
    value_t d21;
    value_t d22;
    value_t d23;
    value_t d24;
    value_t d25;
    value_t d26;
    value_t d27;
    value_t d28;
    value_t d29;
    value_t d30;
    value_t d31;
}

// Full header stack
struct header_t {
    ethernet_h     ethernet;
    arp_h          arp;
    arp_ipv4_h     arp_ipv4;
    ipv4_h         ipv4;
    icmp_h         icmp;
    udp_h          udp;
    switchml_h     switchml;
    ib_bth_h       ib_bth;
    ib_reth_h      ib_reth;
    ib_immediate_h ib_immediate;
    // Two 128-byte data headers to support harvesting 256 bytes with recirculation
    data_h         d0;
    data_h         d1;
    exponents_h    exponents;
    ib_icrc_h      ib_icrc;
}

#endif /* _HEADERS_ */
