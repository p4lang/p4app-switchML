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

#ifndef _BITMAP_CHECKER_
#define _BITMAP_CHECKER_

control ReconstructWorkerBitmap(
    inout ingress_metadata_t ig_md) {

    action reconstruct_worker_bitmap_from_worker_id(worker_bitmap_t bitmap) {
        ig_md.worker_bitmap = bitmap;
    }

    table reconstruct_worker_bitmap {
        key = {
            ig_md.switchml_md.worker_id : ternary;
        }
        actions = {
            reconstruct_worker_bitmap_from_worker_id;
        }
        const entries = {
            0  &&& 0x1f : reconstruct_worker_bitmap_from_worker_id(1 << 0);
            1  &&& 0x1f : reconstruct_worker_bitmap_from_worker_id(1 << 1);
            2  &&& 0x1f : reconstruct_worker_bitmap_from_worker_id(1 << 2);
            3  &&& 0x1f : reconstruct_worker_bitmap_from_worker_id(1 << 3);
            4  &&& 0x1f : reconstruct_worker_bitmap_from_worker_id(1 << 4);
            5  &&& 0x1f : reconstruct_worker_bitmap_from_worker_id(1 << 5);
            6  &&& 0x1f : reconstruct_worker_bitmap_from_worker_id(1 << 6);
            7  &&& 0x1f : reconstruct_worker_bitmap_from_worker_id(1 << 7);
            8  &&& 0x1f : reconstruct_worker_bitmap_from_worker_id(1 << 8);
            9  &&& 0x1f : reconstruct_worker_bitmap_from_worker_id(1 << 9);
            10 &&& 0x1f : reconstruct_worker_bitmap_from_worker_id(1 << 10);
            11 &&& 0x1f : reconstruct_worker_bitmap_from_worker_id(1 << 11);
            12 &&& 0x1f : reconstruct_worker_bitmap_from_worker_id(1 << 12);
            13 &&& 0x1f : reconstruct_worker_bitmap_from_worker_id(1 << 13);
            14 &&& 0x1f : reconstruct_worker_bitmap_from_worker_id(1 << 14);
            15 &&& 0x1f : reconstruct_worker_bitmap_from_worker_id(1 << 15);
            16 &&& 0x1f : reconstruct_worker_bitmap_from_worker_id(1 << 16);
            17 &&& 0x1f : reconstruct_worker_bitmap_from_worker_id(1 << 17);
            18 &&& 0x1f : reconstruct_worker_bitmap_from_worker_id(1 << 18);
            19 &&& 0x1f : reconstruct_worker_bitmap_from_worker_id(1 << 19);
            20 &&& 0x1f : reconstruct_worker_bitmap_from_worker_id(1 << 20);
            21 &&& 0x1f : reconstruct_worker_bitmap_from_worker_id(1 << 21);
            22 &&& 0x1f : reconstruct_worker_bitmap_from_worker_id(1 << 22);
            23 &&& 0x1f : reconstruct_worker_bitmap_from_worker_id(1 << 23);
            24 &&& 0x1f : reconstruct_worker_bitmap_from_worker_id(1 << 24);
            25 &&& 0x1f : reconstruct_worker_bitmap_from_worker_id(1 << 25);
            26 &&& 0x1f : reconstruct_worker_bitmap_from_worker_id(1 << 26);
            27 &&& 0x1f : reconstruct_worker_bitmap_from_worker_id(1 << 27);
            28 &&& 0x1f : reconstruct_worker_bitmap_from_worker_id(1 << 28);
            29 &&& 0x1f : reconstruct_worker_bitmap_from_worker_id(1 << 29);
            30 &&& 0x1f : reconstruct_worker_bitmap_from_worker_id(1 << 30);
            31 &&& 0x1f : reconstruct_worker_bitmap_from_worker_id(1 << 31);
        }
    }

    apply {
        reconstruct_worker_bitmap.apply();
    }
}

control UpdateAndCheckWorkerBitmap(
    inout header_t hdr,
    inout ingress_metadata_t ig_md,
    in ingress_intrinsic_metadata_t ig_intr_md,
    inout ingress_intrinsic_metadata_for_deparser_t ig_dprsr_md,
    inout ingress_intrinsic_metadata_for_tm_t ig_tm_md) {

    Register<worker_bitmap_pair_t, pool_index_by2_t>(num_slots) worker_bitmap;

    RegisterAction<worker_bitmap_pair_t, pool_index_by2_t, worker_bitmap_t>(worker_bitmap) worker_bitmap_update_set0 = {
        void apply(inout worker_bitmap_pair_t value, out worker_bitmap_t return_value) {
            return_value = value.first; // return first set
            value.first  = value.first  | ig_md.worker_bitmap;    // add bit to first set
            value.second = value.second & (~ig_md.worker_bitmap); // remove bit from second set
        }
    };

    RegisterAction<worker_bitmap_pair_t, pool_index_by2_t, worker_bitmap_t>(worker_bitmap) worker_bitmap_update_set1 = {
        void apply(inout worker_bitmap_pair_t value, out worker_bitmap_t return_value) {
            return_value = value.second; // return second set
            value.first  = value.first & (~ig_md.worker_bitmap); // remove bit from first set
            value.second = value.second | ig_md.worker_bitmap;    // add bit to second set
        }
    };

    action drop() {
        // Mark for drop; mark as IGNORE so we don't further process this packet
        ig_dprsr_md.drop_ctl[0:0] = 1;
        ig_md.switchml_md.packet_type = packet_type_t.IGNORE;
    }

    action simulate_drop() {
        drop();
    }

    action check_worker_bitmap_action() {
        // Set map result to nonzero if this packet is a retransmission
        ig_md.switchml_md.map_result = ig_md.switchml_md.worker_bitmap_before & ig_md.worker_bitmap;
    }

    action update_worker_bitmap_set0_action() {
        ig_md.switchml_md.worker_bitmap_before = worker_bitmap_update_set0.execute(ig_md.switchml_md.pool_index[14:1]);
        check_worker_bitmap_action();
    }

    action update_worker_bitmap_set1_action() {
        ig_md.switchml_md.worker_bitmap_before = worker_bitmap_update_set1.execute(ig_md.switchml_md.pool_index[14:1]);
        check_worker_bitmap_action();
    }

    table update_and_check_worker_bitmap {
        key = {
            ig_md.switchml_md.pool_index : ternary;
            ig_md.switchml_md.packet_type : ternary; // only act on packets of type CONSUME0
            ig_md.port_metadata.ingress_drop_probability : ternary; // if nonzero, drop packet
        }
        actions = {
            update_worker_bitmap_set0_action;
            update_worker_bitmap_set1_action;
            drop;
            simulate_drop;
            NoAction;
        }
        const entries = {
            // Drop packets indicated by the drop simulator
            (            _, packet_type_t.CONSUME0, 0xffff) : simulate_drop();

            // Direct updates to the correct set
            (15w0 &&& 15w1, packet_type_t.CONSUME0,      _) : update_worker_bitmap_set0_action();
            (15w1 &&& 15w1, packet_type_t.CONSUME0,      _) : update_worker_bitmap_set1_action();

            (15w0 &&& 15w1, packet_type_t.CONSUME1,      _) : update_worker_bitmap_set0_action();
            (15w1 &&& 15w1, packet_type_t.CONSUME1,      _) : update_worker_bitmap_set1_action();

            (15w0 &&& 15w1, packet_type_t.CONSUME2,      _) : update_worker_bitmap_set0_action();
            (15w1 &&& 15w1, packet_type_t.CONSUME2,      _) : update_worker_bitmap_set1_action();

            (15w0 &&& 15w1, packet_type_t.CONSUME3,      _) : update_worker_bitmap_set0_action();
            (15w1 &&& 15w1, packet_type_t.CONSUME3,      _) : update_worker_bitmap_set1_action();
        }
        const default_action = NoAction;
    }

    apply {
        update_and_check_worker_bitmap.apply();
    }
}

#endif /* _BITMAP_CHECKER_ */
