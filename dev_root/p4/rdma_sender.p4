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

#ifndef _RDMA_SENDER_
#define _RDMA_SENDER_

control RDMASender(
    inout header_t hdr,
    inout egress_metadata_t eg_md,
    in egress_intrinsic_metadata_t eg_intr_md,
    in egress_intrinsic_metadata_from_parser_t eg_intr_md_from_prsr,
    inout egress_intrinsic_metadata_for_deparser_t eg_intr_dprs_md) {

    // Temporary variables
    rkey_t rdma_rkey;
    mac_addr_t rdma_switch_mac;
    ipv4_addr_t rdma_switch_ip;

    DirectCounter<counter_t>(CounterType_t.PACKETS_AND_BYTES) rdma_send_counter;

    // Read switch MAC and IP from table to form output packets
    action set_switch_mac_and_ip(
        mac_addr_t switch_mac, ipv4_addr_t switch_ip,
        bit<31> message_length,
        pool_index_t first_last_mask) {

        // Record switch addresses
        rdma_switch_mac = switch_mac;
        rdma_switch_ip = switch_ip;
    }

    table switch_mac_and_ip {
        actions = { @defaultonly set_switch_mac_and_ip; }
        size = 1;
    }

    // Get destination MAC and IP and form RoCE output packets
    // (Sequence number and queue pair will be filled in later)
    action fill_in_roce_fields(mac_addr_t dest_mac, ipv4_addr_t dest_ip) {
        // Ensure we don't have a switchml header
        hdr.switchml.setInvalid();
        hdr.exponents.setInvalid();

        hdr.ethernet.setValid();
        hdr.ethernet.dst_addr = dest_mac;
        hdr.ethernet.src_addr = rdma_switch_mac;
        hdr.ethernet.ether_type = ETHERTYPE_IPV4;

        hdr.ipv4.setValid();
        hdr.ipv4.version = 4;
        hdr.ipv4.ihl = 5;
        hdr.ipv4.diffserv = 0x02;
        hdr.ipv4.identification = 0x0001;
        hdr.ipv4.flags = 0b010;
        hdr.ipv4.frag_offset = 0;
        hdr.ipv4.ttl = 64;
        hdr.ipv4.protocol = ip_protocol_t.UDP;
        hdr.ipv4.hdr_checksum = 0; // To be filled in by deparser
        hdr.ipv4.src_addr = rdma_switch_ip;
        hdr.ipv4.dst_addr = dest_ip;

        // Set base IPv4 packet length; will be updated later based on
        // payload size and headers
        hdr.ipv4.total_len = ( \
            hdr.ib_icrc.minSizeInBytes() + \
            hdr.ib_bth.minSizeInBytes() + \
            hdr.udp.minSizeInBytes() + \
            hdr.ipv4.minSizeInBytes());

        // Update IPv4 checksum
        eg_md.update_ipv4_checksum = true;

        hdr.udp.setValid();
        hdr.udp.src_port = 1w1 ++ eg_md.switchml_md.worker_id[14:0]; // form a consistent source port for this worker
        hdr.udp.dst_port = UDP_PORT_ROCEV2;
        hdr.udp.checksum = 0; // disabled for RoCEv2

        // Set base UDP packet length; will be updated later based on
        // payload size and headers
        hdr.udp.length = ( \
            hdr.ib_icrc.minSizeInBytes() + \
            hdr.ib_bth.minSizeInBytes() + \
            hdr.udp.minSizeInBytes());


        hdr.ib_bth.setValid();
        hdr.ib_bth.opcode = ib_opcode_t.UC_RDMA_WRITE_ONLY; // to be filled in later
        hdr.ib_bth.se = 0;
        hdr.ib_bth.migration_req = 1;
        hdr.ib_bth.pad_count = 0;
        hdr.ib_bth.transport_version = 0;
        hdr.ib_bth.partition_key = 0xffff;
        hdr.ib_bth.f_res1 = 0;
        hdr.ib_bth.b_res1 = 0;
        hdr.ib_bth.reserved = 0;
        hdr.ib_bth.dst_qp = 0; // to be filled in later
        hdr.ib_bth.ack_req = 0;
        hdr.ib_bth.reserved2 = 0;

        // NOTE: we don't add an ICRC header here for two reasons:
        // 1. we haven't parsed the payload in egress, so we can't place it at the right point
        // 2. the payload may be too big for us to parse (1024B packets)
        // Thus, we just leave the existing ICRC in the packet buffer
        // during ingress processing, and leave it at the right point
        // in the egress packet. This works because we're having the NICs ignore it.

        // Count send
        rdma_send_counter.count();
    }

    action fill_in_roce_write_fields(mac_addr_t dest_mac, ipv4_addr_t dest_ip, rkey_t rkey) {
        fill_in_roce_fields(dest_mac, dest_ip);

        rdma_rkey = rkey;
    }

    table create_roce_packet {
        key = {
            eg_md.switchml_md.worker_id : exact;
        }
        actions = {
            fill_in_roce_fields;
            fill_in_roce_write_fields;
        }
        size = max_num_workers;
        counters = rdma_send_counter;
    }

    DirectRegister<bit<32>>() psn_register;

    // This will be initialized through control plane
    DirectRegisterAction<bit<32>, bit<32>>(psn_register) psn_action = {

        void apply(inout bit<32> value, out bit<32> read_value) {
            // Emit 24-bit sequence number
            bit<32> masked_sequence_number = value & 0x00ffffff;
            read_value = masked_sequence_number;

            // Increment sequence number
            bit<32> incremented_value = value + 1;
            value = incremented_value;
        }
    };

    action add_qpn_and_psn(queue_pair_t qpn) {
        hdr.ib_bth.dst_qp = qpn;
        hdr.ib_bth.psn = psn_action.execute()[23:0];
    }

    table fill_in_qpn_and_psn {
        key = {
            eg_md.switchml_md.worker_id  : exact; // replication ID: indicates which worker we're sending to
            eg_md.switchml_md.pool_index : ternary;
        }
        actions = {
            add_qpn_and_psn;
        }
        size = max_num_queue_pairs;
        registers = psn_register;
    }

    action set_opcode_common(ib_opcode_t opcode) {
        hdr.ib_bth.opcode = opcode;
    }

    action set_immediate() {
        hdr.ib_immediate.setValid();
        hdr.ib_immediate.immediate = 0x12345678; // TODO: Exponents in immediate
    }

    action set_rdma() {
        hdr.ib_reth.setValid();
        hdr.ib_reth.r_key = rdma_rkey;
        hdr.ib_reth.len = eg_md.switchml_md.tsi;
        hdr.ib_reth.addr = eg_md.switchml_rdma_md.rdma_addr;
    }

    action set_middle() {
        set_opcode_common(ib_opcode_t.UC_RDMA_WRITE_MIDDLE);
        // use default adjusted length for UDP and IPv4 headers
    }

    action set_last_immediate() {
        set_opcode_common(ib_opcode_t.UC_RDMA_WRITE_LAST_IMMEDIATE);
        set_immediate();

        hdr.udp.length = hdr.udp.length + (bit<16>) hdr.ib_immediate.minSizeInBytes();
        hdr.ipv4.total_len = hdr.ipv4.total_len + (bit<16>) hdr.ib_immediate.minSizeInBytes();
    }

    action set_first() {
        set_opcode_common(ib_opcode_t.UC_RDMA_WRITE_FIRST);
        set_rdma();

        hdr.udp.length = hdr.udp.length + (bit<16>) hdr.ib_reth.minSizeInBytes();
        hdr.ipv4.total_len = hdr.ipv4.total_len + (bit<16>) hdr.ib_reth.minSizeInBytes();
    }

    action set_only_immediate() {
        set_opcode_common(ib_opcode_t.UC_RDMA_WRITE_ONLY_IMMEDIATE);
        set_rdma();
        set_immediate();

        hdr.udp.length = hdr.udp.length + (bit<16>) (hdr.ib_immediate.minSizeInBytes() + hdr.ib_reth.minSizeInBytes());
        hdr.ipv4.total_len = hdr.ipv4.total_len + (bit<16>) (hdr.ib_immediate.minSizeInBytes() + hdr.ib_reth.minSizeInBytes());
    }

    table set_opcodes {
        key = {
            eg_md.switchml_rdma_md.first_packet : exact;
            eg_md.switchml_rdma_md.last_packet : exact;
        }
        actions = {
            set_first;
            set_middle;
            set_last_immediate;
            set_only_immediate;
        }
        size = 4;
        const entries = {
            ( true, false) :          set_first(); // RDMA_WRITE_FIRST;
            (false, false) :         set_middle(); // RDMA_WRITE_MIDDLE;
            (false,  true) : set_last_immediate(); // RDMA_WRITE_LAST_IMMEDIATE;
            ( true,  true) : set_only_immediate(); // RDMA_WRITE_ONLY_IMMEDIATE;
        }
    }

    apply {
        // Get switch IP and switch MAC
        switch_mac_and_ip.apply();

        // Fill in headers for ROCE packet
        create_roce_packet.apply();

        // Add payload size
        if (eg_md.switchml_md.packet_size == packet_size_t.IBV_MTU_256) {
            hdr.ipv4.total_len = hdr.ipv4.total_len + 256;
            hdr.udp.length = hdr.udp.length + 256;
        }
        else if (eg_md.switchml_md.packet_size == packet_size_t.IBV_MTU_1024) {
            hdr.ipv4.total_len = hdr.ipv4.total_len + 1024;
            hdr.udp.length = hdr.udp.length + 1024;
        }

        // Fill in queue pair number and sequence number
        fill_in_qpn_and_psn.apply();

        // Fill in opcode based on pool index
        set_opcodes.apply();
    }
}

#endif /* _RDMA_SENDER_ */
