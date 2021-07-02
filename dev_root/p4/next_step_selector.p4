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

#ifndef _NEXT_STEP_SELECTOR_
#define _NEXT_STEP_SELECTOR_

#include "configuration.p4"
#include "types.p4"
#include "headers.p4"

control NextStepSelector(
    inout header_t hdr,
    inout ingress_metadata_t ig_md,
    in ingress_intrinsic_metadata_t ig_intr_md,
    inout ingress_intrinsic_metadata_for_deparser_t ig_dprsr_md,
    inout ingress_intrinsic_metadata_for_tm_t ig_tm_md) {

    bool count_consume;
    bool count_broadcast;
    bool count_retransmit;
    bool count_recirculate;
    bool count_drop;

    Counter<counter_t, pool_index_t>(register_size, CounterType_t.PACKETS) broadcast_counter;
    Counter<counter_t, pool_index_t>(register_size, CounterType_t.PACKETS) retransmit_counter;
    Counter<counter_t, pool_index_t>(register_size, CounterType_t.PACKETS) recirculate_counter;
    Counter<counter_t, pool_index_t>(register_size, CounterType_t.PACKETS) drop_counter;

    action recirculate_for_consume(packet_type_t packet_type, PortId_t recirc_port) {
        // Drop both data headers now that they've been consumed
        hdr.d0.setInvalid();
        hdr.d1.setInvalid();

        // Send to recirculation port
        ig_tm_md.ucast_egress_port = recirc_port;
        ig_tm_md.bypass_egress = 1w1;
        ig_dprsr_md.drop_ctl[0:0] = 0;
        ig_md.switchml_md.packet_type = packet_type;

        count_consume = true;
        count_recirculate = true;
    }

    action recirculate_for_harvest(packet_type_t packet_type, PortId_t recirc_port) {
        // Recirculate for harvest
        ig_tm_md.ucast_egress_port = recirc_port;
        ig_tm_md.bypass_egress = 1w1;
        ig_dprsr_md.drop_ctl[0:0] = 0;
        ig_md.switchml_md.packet_type = packet_type;
    }

    action recirculate_for_CONSUME1(PortId_t recirc_port) {
        recirculate_for_consume(packet_type_t.CONSUME1, recirc_port);
    }

    action recirculate_for_CONSUME2_same_port_next_pipe() {
        recirculate_for_consume(packet_type_t.CONSUME2, 2w2 ++ ig_intr_md.ingress_port[6:0]);
    }

    action recirculate_for_CONSUME3_same_port_next_pipe() {
        recirculate_for_consume(packet_type_t.CONSUME3, 2w3 ++ ig_intr_md.ingress_port[6:0]);
    }

    action recirculate_for_HARVEST1(PortId_t recirc_port) {
        hdr.d0.setInvalid();
        recirculate_for_harvest(packet_type_t.HARVEST1, recirc_port);
    }

    action recirculate_for_HARVEST2(PortId_t recirc_port) {
        hdr.d1.setInvalid();
        recirculate_for_harvest(packet_type_t.HARVEST2, recirc_port);
    }

    action recirculate_for_HARVEST3(PortId_t recirc_port) {
        hdr.d0.setInvalid();
        recirculate_for_harvest(packet_type_t.HARVEST3, recirc_port);
    }

    action recirculate_for_HARVEST4(PortId_t recirc_port) {
        hdr.d1.setInvalid();
        recirculate_for_harvest(packet_type_t.HARVEST4, recirc_port);
    }

    action recirculate_for_HARVEST5(PortId_t recirc_port) {
        hdr.d0.setInvalid();
        recirculate_for_harvest(packet_type_t.HARVEST5, recirc_port);
    }

    action recirculate_for_HARVEST6(PortId_t recirc_port) {
        hdr.d1.setInvalid();
        recirculate_for_harvest(packet_type_t.HARVEST6, recirc_port);
    }

    action recirculate_for_HARVEST7(PortId_t recirc_port) {
        hdr.d0.setInvalid();
        recirculate_for_harvest(packet_type_t.HARVEST7, recirc_port);
    }

    action finish_consume() {
        ig_dprsr_md.drop_ctl[0:0] = 1;
        count_consume = true;
        count_drop = true;
    }

    action broadcast() {
        hdr.d1.setInvalid();

        // Set the switch as the source MAC address
        hdr.ethernet.src_addr = hdr.ethernet.dst_addr;
        // Destination address will be filled in egress pipe

        // Send to multicast group; egress will fill in destination IP and MAC address
        ig_tm_md.mcast_grp_a = ig_md.switchml_md.mgid;
        ig_tm_md.level1_exclusion_id = null_level1_exclusion_id; // don't exclude any nodes
        ig_md.switchml_md.packet_type = packet_type_t.BROADCAST;
        ig_tm_md.bypass_egress = 1w0;
        ig_dprsr_md.drop_ctl[0:0] = 0;

        count_broadcast = true;
    }

    action retransmit() {
        hdr.d1.setInvalid();

        // Send back out ingress port
        ig_tm_md.ucast_egress_port = ig_md.switchml_md.ingress_port;
        ig_md.switchml_md.packet_type = packet_type_t.RETRANSMIT;
        ig_tm_md.bypass_egress = 1w0;
        ig_dprsr_md.drop_ctl[0:0] = 0;

        count_retransmit = true;
    }

    action drop() {
        // Mark for drop
        ig_dprsr_md.drop_ctl[0:0] = 1;
        ig_md.switchml_md.packet_type = packet_type_t.IGNORE;
        count_drop = true;
    }

    table next_step {
        key = {
            ig_md.switchml_md.packet_size : ternary;
            ig_md.switchml_md.worker_id : ternary;
            ig_md.switchml_md.packet_type : ternary;
            ig_md.switchml_md.first_last_flag : ternary; // 1: last 0: first
            ig_md.switchml_md.map_result : ternary;
        }
        actions = {
            recirculate_for_CONSUME1;
            recirculate_for_CONSUME2_same_port_next_pipe;
            recirculate_for_CONSUME3_same_port_next_pipe;
            recirculate_for_HARVEST1;
            recirculate_for_HARVEST2;
            recirculate_for_HARVEST3;
            recirculate_for_HARVEST4;
            recirculate_for_HARVEST5;
            recirculate_for_HARVEST6;
            recirculate_for_HARVEST7;
            finish_consume;
            broadcast;
            retransmit;
            drop;
        }
        const default_action = drop();
        size = 128;
    }

    apply {

        count_consume = false;
        count_broadcast = false;
        count_retransmit = false;
        count_recirculate = false;
        count_drop = false;

        next_step.apply();

        // Update counters
        if (count_consume || count_drop) {
            drop_counter.count(ig_md.switchml_md.pool_index);
        }

        if (count_recirculate) {
            recirculate_counter.count(ig_md.switchml_md.pool_index);
        }

        if (count_broadcast) {
            broadcast_counter.count(ig_md.switchml_md.pool_index);
        }

        if (count_retransmit) {
            retransmit_counter.count(ig_md.switchml_md.pool_index);
        }
    }
}

#endif /* _NEXT_STEP_SELECTOR_ */
