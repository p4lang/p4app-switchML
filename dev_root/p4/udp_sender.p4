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

#ifndef _UDP_SENDER_
#define _UDP_SENDER_

#define UDP_LENGTH (hdr.udp.minSizeInBytes() + hdr.switchml.minSizeInBytes() + hdr.exponents.minSizeInBytes())
#define IPV4_LENGTH (hdr.ipv4.minSizeInBytes() + UDP_LENGTH);

control UDPSender(
    inout egress_metadata_t eg_md,
    in egress_intrinsic_metadata_t eg_intr_md,
    inout header_t hdr) {

    DirectCounter<counter_t>(CounterType_t.PACKETS_AND_BYTES) send_counter;

    // Read switch MAC and IP from table to form output packets
    action set_switch_mac_and_ip(mac_addr_t switch_mac, ipv4_addr_t switch_ip) {

        // Set switch addresses
        hdr.ethernet.src_addr = switch_mac;
        hdr.ipv4.src_addr = switch_ip;

        hdr.udp.src_port = eg_md.switchml_md.src_port;

        hdr.ethernet.ether_type = ETHERTYPE_IPV4;

        hdr.ipv4.version = 4;
        hdr.ipv4.ihl = 5;
        hdr.ipv4.diffserv = 0x00;
        hdr.ipv4.total_len = IPV4_LENGTH;
        hdr.ipv4.identification = 0x0000;
        hdr.ipv4.flags = 0b000;
        hdr.ipv4.frag_offset = 0;
        hdr.ipv4.ttl = 64;
        hdr.ipv4.protocol = ip_protocol_t.UDP;
        hdr.ipv4.hdr_checksum = 0; // To be filled in by deparser
        hdr.ipv4.src_addr = switch_ip;
        eg_md.update_ipv4_checksum = true;

        hdr.udp.length = UDP_LENGTH;

        hdr.switchml.setValid();
        hdr.switchml.msg_type = 1;
        hdr.switchml.unused = 0;
        hdr.switchml.size = eg_md.switchml_md.packet_size;
        hdr.switchml.job_number = eg_md.switchml_md.job_number;
        hdr.switchml.tsi = eg_md.switchml_md.tsi;

        // Rearrange pool index
        hdr.switchml.pool_index[13:0] = eg_md.switchml_md.pool_index[14:1];
    }

    table switch_mac_and_ip {
        actions = { @defaultonly set_switch_mac_and_ip; }
        size = 1;
    }

    action set_dst_addr(
        mac_addr_t eth_dst_addr,
        ipv4_addr_t ip_dst_addr) {

        // Set to destination node
        hdr.ethernet.dst_addr = eth_dst_addr;
        hdr.ipv4.dst_addr = ip_dst_addr;

        hdr.udp.dst_port = eg_md.switchml_md.dst_port;

        // Disable UDP checksum for now
        hdr.udp.checksum = 0;

        // Update IPv4 checksum
        eg_md.update_ipv4_checksum = true;

        // Pool set bit
        hdr.switchml.pool_index[15:15] = eg_md.switchml_md.pool_index[0:0];

        // Exponents
        hdr.exponents.setValid();
        hdr.exponents.e0 = eg_md.switchml_md.e0;
        hdr.exponents.e1 = eg_md.switchml_md.e1;

        // Count send
        send_counter.count();
    }

    table dst_addr {
        key = {
            eg_md.switchml_md.worker_id : exact;
        }
        actions = {
            set_dst_addr;
        }
        size = max_num_workers;
        counters = send_counter;
    }

    apply {
        hdr.ethernet.setValid();
        hdr.ipv4.setValid();
        hdr.udp.setValid();
        hdr.switchml.setValid();
        hdr.switchml.pool_index = 16w0;

        switch_mac_and_ip.apply();
        dst_addr.apply();

        // Add payload size
        if (eg_md.switchml_md.packet_size == packet_size_t.IBV_MTU_256) {
            hdr.ipv4.total_len = hdr.ipv4.total_len + 256;
            hdr.udp.length = hdr.udp.length + 256;
        }
        else if (eg_md.switchml_md.packet_size == packet_size_t.IBV_MTU_1024) {
            hdr.ipv4.total_len = hdr.ipv4.total_len + 1024;
            hdr.udp.length = hdr.udp.length + 1024;
        }
    }
}

#endif /* _UDP_SENDER_ */
