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

#ifndef _UDP_RECEIVER_
#define _UDP_RECEIVER_

#include "configuration.p4"
#include "types.p4"
#include "headers.p4"

control UDPReceiver(
    inout header_t hdr,
    inout ingress_metadata_t ig_md,
    in ingress_intrinsic_metadata_t ig_intr_md,
    in ingress_intrinsic_metadata_from_parser_t ig_prsr_md,
    inout ingress_intrinsic_metadata_for_deparser_t ig_dprsr_md,
    inout ingress_intrinsic_metadata_for_tm_t ig_tm_md) {

    DirectCounter<counter_t>(CounterType_t.PACKETS_AND_BYTES) receive_counter;

    // Packet was received with errors; set drop bit in deparser metadata
    action drop() {
        // Ignore this packet and drop when it leaves pipeline
        ig_dprsr_md.drop_ctl[0:0] = 1;
        ig_md.switchml_md.packet_type = packet_type_t.IGNORE;
        receive_counter.count();
    }

    // This is a regular packet; just forward
    action forward() {
        ig_md.switchml_md.packet_type = packet_type_t.IGNORE;
        receive_counter.count();
    }

    action set_bitmap(
        MulticastGroupId_t mgid,
        worker_type_t worker_type,
        worker_id_t worker_id,
        num_workers_t num_workers,
        worker_bitmap_t worker_bitmap) {

        // Count received packet
        receive_counter.count();

        // Bitmap representation for this worker
        ig_md.worker_bitmap = worker_bitmap;
        ig_md.switchml_md.num_workers = num_workers;

        // Group ID for this job
        ig_md.switchml_md.mgid = mgid;

        // Record packet size for use in recirculation
        ig_md.switchml_md.packet_size = hdr.switchml.size;

        ig_md.switchml_md.worker_type = worker_type;
        ig_md.switchml_md.worker_id = worker_id;
        ig_md.switchml_md.dst_port = hdr.udp.src_port;
        ig_md.switchml_md.src_port = hdr.udp.dst_port;
        ig_md.switchml_md.tsi = hdr.switchml.tsi;
        ig_md.switchml_md.job_number = hdr.switchml.job_number;

        // Move the SwitchML set bit in the MSB to the LSB. TODO move set bit to MSB
        ig_md.switchml_md.pool_index = hdr.switchml.pool_index[13:0] ++ hdr.switchml.pool_index[15:15];

        // Mark packet as single-packet message since it's the UDP protocol
        ig_md.switchml_md.first_packet = true;
        ig_md.switchml_md.last_packet = true;

        // Exponents
        ig_md.switchml_md.e0 = hdr.exponents.e0;
        ig_md.switchml_md.e1 = hdr.exponents.e1;

        // Get rid of headers we don't want to recirculate
        hdr.ethernet.setInvalid();
        hdr.ipv4.setInvalid();
        hdr.udp.setInvalid();
        hdr.switchml.setInvalid();
        hdr.exponents.setInvalid();
    }

    table receive_udp {
        key = {
            // use ternary matches to support matching on:
            // * ingress port only like the original design
            // * source IP and UDP destination port for the SwitchML Eth protocol
            // * source IP and UDP destination port for the SwitchML UDP protocol
            // * source IP and destination QP number for the RoCE protocols
            // * also, parser error values so we can drop bad packets
            ig_intr_md.ingress_port   : ternary;
            hdr.ethernet.src_addr     : ternary;
            hdr.ethernet.dst_addr     : ternary;
            hdr.ipv4.src_addr         : ternary;
            hdr.ipv4.dst_addr         : ternary;
            hdr.udp.dst_port          : ternary;
            ig_prsr_md.parser_err     : ternary;
        }

        actions = {
            drop;
            set_bitmap;
            @defaultonly forward;
        }
        const default_action = forward;

        // Create some extra table space to support parser error entries
        size = max_num_workers + 16;

        // Count received packets
        counters = receive_counter;
    }

    apply {
        receive_udp.apply();
    }
}

#endif /* _UDP_RECEIVER_ */
