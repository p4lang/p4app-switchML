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

#ifndef _PARSERS_
#define _PARSERS_

#include "types.p4"
#include "headers.p4"

parser IngressParser(
    packet_in pkt,
    out header_t hdr,
    out ingress_metadata_t ig_md,
    out ingress_intrinsic_metadata_t ig_intr_md) {

    Checksum() ipv4_checksum;

    state start {
        pkt.extract(ig_intr_md);
        transition select(ig_intr_md.resubmit_flag) {
            1 : parse_resubmit;
            default : parse_port_metadata;
        }
    }

    state parse_resubmit {
        // Resubmission not currently used; just skip header
        // assume recirculated packets will never be resubmitted for now
    	pkt.advance(64);
        transition parse_ethernet;
    }

    state parse_port_metadata {
        // parse port metadata
        ig_md.port_metadata = port_metadata_unpack<port_metadata_t>(pkt);

        transition select(ig_intr_md.ingress_port) {
            64: parse_recirculate; // pipe 0 CPU Eth port
            68: parse_recirculate; // pipe 0 recirc port
            320: parse_ethernet;   // pipe 2 CPU PCIe port
            0x080 &&& 0x180: parse_recirculate; // all pipe 1 ports
            0x100 &&& 0x180: parse_recirculate; // all pipe 2 ports
            0x180 &&& 0x180: parse_recirculate; // all pipe 3 ports
            default:  parse_ethernet;
        }
    }

    state parse_recirculate {
        // Parse switchml metadata
        pkt.extract(ig_md.switchml_md);
        pkt.extract(ig_md.switchml_rdma_md);

        // Now parse the rest of the packet
        transition select(ig_md.switchml_md.packet_type) {
            (packet_type_t.CONSUME0) : parse_consume;
            (packet_type_t.CONSUME1) : parse_consume;
            (packet_type_t.CONSUME2) : parse_consume;
            (packet_type_t.CONSUME3) : parse_consume;
            default : parse_harvest; // default to parsing for harvests
        }
    }

    state parse_consume {
        // Extract the next 256B values
        pkt.extract(hdr.d0);
        pkt.extract(hdr.d1);
        transition accept;
    }

    state parse_harvest {
        // One of these will be filled in by the pipeline, and the other set invalid
        hdr.d0.setValid();
        hdr.d1.setValid();
        transition accept;
    }

    state parse_ethernet {
        pkt.extract(hdr.ethernet);
        transition select(hdr.ethernet.ether_type) {
            ETHERTYPE_ARP : parse_arp;
            ETHERTYPE_IPV4 : parse_ipv4;
            default : accept_regular;
        }
    }

    state parse_arp {
        pkt.extract(hdr.arp);
        transition select(hdr.arp.hw_type, hdr.arp.proto_type) {
            (0x0001, ETHERTYPE_IPV4) : parse_arp_ipv4;
            default: accept_regular;
        }
    }

    state parse_arp_ipv4 {
        pkt.extract(hdr.arp_ipv4);
        transition accept_regular;
    }

    state parse_ipv4 {
        pkt.extract(hdr.ipv4);
        ipv4_checksum.add(hdr.ipv4);
        ig_md.checksum_err_ipv4 = ipv4_checksum.verify();
        ig_md.update_ipv4_checksum = false;

        // parse only non-fragmented IP packets with no options
        transition select(hdr.ipv4.ihl, hdr.ipv4.frag_offset, hdr.ipv4.protocol) {
            (5, 0, ip_protocol_t.ICMP) : parse_icmp;
            (5, 0, ip_protocol_t.UDP)  : parse_udp;
            default                    : accept_regular;
        }
    }

    state parse_icmp {
        pkt.extract(hdr.icmp);
        transition accept_regular;
    }

    state parse_udp {
        pkt.extract(hdr.udp);
        transition select(hdr.udp.dst_port) {
            UDP_PORT_ROCEV2                                   : parse_ib_bth;
            UDP_PORT_SWITCHML_BASE &&& UDP_PORT_SWITCHML_MASK : parse_switchml;
            default                                           : accept_regular;
        }
    }

    state parse_ib_bth {
        pkt.extract(hdr.ib_bth);
        transition select(hdr.ib_bth.opcode) {
            // include only UC operations here
            ib_opcode_t.UC_SEND_FIRST                : parse_ib_payload;
            ib_opcode_t.UC_SEND_MIDDLE               : parse_ib_payload;
            ib_opcode_t.UC_SEND_LAST                 : parse_ib_payload;
            ib_opcode_t.UC_SEND_LAST_IMMEDIATE       : parse_ib_immediate;
            ib_opcode_t.UC_SEND_ONLY                 : parse_ib_payload;
            ib_opcode_t.UC_SEND_ONLY_IMMEDIATE       : parse_ib_immediate;
            ib_opcode_t.UC_RDMA_WRITE_FIRST          : parse_ib_reth;
            ib_opcode_t.UC_RDMA_WRITE_MIDDLE         : parse_ib_payload;
            ib_opcode_t.UC_RDMA_WRITE_LAST           : parse_ib_payload;
            ib_opcode_t.UC_RDMA_WRITE_LAST_IMMEDIATE : parse_ib_immediate;
            ib_opcode_t.UC_RDMA_WRITE_ONLY           : parse_ib_reth;
            ib_opcode_t.UC_RDMA_WRITE_ONLY_IMMEDIATE : parse_ib_reth_immediate;
            default: accept_regular;
        }
    }

    state parse_ib_immediate {
        pkt.extract(hdr.ib_immediate);
        transition parse_ib_payload;
    }

    state parse_ib_reth {
        pkt.extract(hdr.ib_reth);
        transition parse_ib_payload;
    }

    state parse_ib_reth_immediate {
        pkt.extract(hdr.ib_reth);
        pkt.extract(hdr.ib_immediate);
        transition parse_ib_payload;
    }

    state parse_ib_payload {
        pkt.extract(hdr.d0);
        pkt.extract(hdr.d1);
        // do NOT extract ICRC, since this might be in the middle of a >256B packet
        ig_md.switchml_md.setValid();
        ig_md.switchml_md.ether_type_msb = 16w0xffff;
        ig_md.switchml_md.packet_type = packet_type_t.CONSUME0;
        ig_md.switchml_rdma_md.setValid();
        transition accept;
    }

    state parse_switchml {
        pkt.extract(hdr.switchml);
        transition parse_values;
    }

    state parse_values {
        pkt.extract(hdr.d0);
        pkt.extract(hdr.d1);
        pkt.extract(hdr.exponents);
        // At this point we know this is a SwitchML packet that wasn't recirculated,
        // so mark it for consumption
        ig_md.switchml_md.setValid();
        ig_md.switchml_md.packet_type = packet_type_t.CONSUME0;
        ig_md.switchml_rdma_md.setValid();
        transition accept;
    }

    state accept_regular {
        ig_md.switchml_md.setValid();
        ig_md.switchml_md.packet_type = packet_type_t.IGNORE;
        ig_md.switchml_rdma_md.setValid();
        transition accept;
    }
}

control IngressDeparser(
    packet_out pkt,
    inout header_t hdr,
    in ingress_metadata_t ig_md,
    in ingress_intrinsic_metadata_for_deparser_t ig_dprsr_md) {

    Checksum() ipv4_checksum;

    apply {
        if (ig_md.update_ipv4_checksum) {
            hdr.ipv4.hdr_checksum = ipv4_checksum.update({
                    hdr.ipv4.version,
                    hdr.ipv4.ihl,
                    hdr.ipv4.diffserv,
                    hdr.ipv4.total_len,
                    hdr.ipv4.identification,
                    hdr.ipv4.flags,
                    hdr.ipv4.frag_offset,
                    hdr.ipv4.ttl,
                    hdr.ipv4.protocol,
                    hdr.ipv4.src_addr,
                    hdr.ipv4.dst_addr});
        }

        pkt.emit(ig_md.switchml_md);
        pkt.emit(ig_md.switchml_rdma_md);
        pkt.emit(hdr);
    }
}

parser EgressParser(
    packet_in pkt,
    out header_t hdr,
    out egress_metadata_t eg_md,
    out egress_intrinsic_metadata_t eg_intr_md) {

    state start {
        pkt.extract(eg_intr_md);
        // All egress packets have a bridged metadata header
        transition select(eg_intr_md.pkt_length) {
            0 : parse_switchml_md;
            _ : parse_switchml_md;
        }
    }

    state parse_switchml_md {
        pkt.extract(eg_md.switchml_md);
        transition parse_switchml_rdma_md;
    }

    state parse_switchml_rdma_md {
        pkt.extract(eg_md.switchml_rdma_md);
        transition accept;
    }
}

control EgressDeparser(
    packet_out pkt,
    inout header_t hdr,
    in egress_metadata_t eg_md,
    in egress_intrinsic_metadata_for_deparser_t eg_intr_dprs_md) {

    Checksum() ipv4_checksum;

    apply {
        if (eg_md.update_ipv4_checksum) {
            hdr.ipv4.hdr_checksum = ipv4_checksum.update({
                    hdr.ipv4.version,
                    hdr.ipv4.ihl,
                    hdr.ipv4.diffserv,
                    hdr.ipv4.total_len,
                    hdr.ipv4.identification,
                    hdr.ipv4.flags,
                    hdr.ipv4.frag_offset,
                    hdr.ipv4.ttl,
                    hdr.ipv4.protocol,
                    hdr.ipv4.src_addr,
                    hdr.ipv4.dst_addr});
        }

        pkt.emit(hdr);
    }
}

#endif /* _PARSERS_ */
