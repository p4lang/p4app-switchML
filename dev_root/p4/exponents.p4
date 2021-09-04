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

#ifndef _EXPONENTS_
#define _EXPONENTS_

#include "types.p4"
#include "headers.p4"

// Exponents max value calculator
// Each control handles two exponents
control Exponents(
    in exponent_t exponent0,
    in exponent_t exponent1,
    out exponent_t max_exponent0,
    out exponent_t max_exponent1,
    inout ingress_metadata_t ig_md) {

    Register<exponent_pair_t, pool_index_t>(register_size) exponents;

    // Write both exponents and read first one
    RegisterAction<exponent_pair_t, pool_index_t, exponent_t>(exponents) write_read0_register_action = {
        void apply(inout exponent_pair_t value, out exponent_t read_value) {
            value.first = exponent0;
            value.second = exponent1;
            read_value = value.first;
        }
    };

    action write_read0_action() {
        max_exponent0 = write_read0_register_action.execute(ig_md.switchml_md.pool_index);
    }

    // Compute max of both exponents and read first one
    RegisterAction<exponent_pair_t, pool_index_t, exponent_t>(exponents) max_read0_register_action = {
        void apply(inout exponent_pair_t value, out exponent_t read_value) {
            value.first  = max(value.first,  exponent0);
            value.second = max(value.second, exponent1);
            read_value = value.first;
        }
    };

    action max_read0_action() {
        max_exponent0 = max_read0_register_action.execute(ig_md.switchml_md.pool_index);
    }

    // Read first max register
    RegisterAction<exponent_pair_t, pool_index_t, exponent_t>(exponents) read0_register_action = {
        void apply(inout exponent_pair_t value, out exponent_t read_value) {
            read_value = value.first;
        }
    };

    action read0_action() {
        max_exponent0 = read0_register_action.execute(ig_md.switchml_md.pool_index);
    }

    // Read second max register
    RegisterAction<exponent_pair_t, pool_index_t, exponent_t>(exponents) read1_register_action = {
        void apply(inout exponent_pair_t value, out exponent_t read_value) {
            read_value = value.second;
        }
    };

    action read1_action() {
        max_exponent1 = read1_register_action.execute(ig_md.switchml_md.pool_index);
    }

    table exponent_max {
        key = {
            ig_md.switchml_md.worker_bitmap_before : ternary;
            ig_md.switchml_md.map_result : ternary;
            ig_md.switchml_md.packet_type: ternary;
        }
        actions = {
            write_read0_action;
            max_read0_action;
            read0_action;
            read1_action;
            @defaultonly NoAction;
        }
        size = 4;
        const entries = {
            // If bitmap_before is all 0's and type is CONSUME0, this is the first packet for slot,
            // so just write values and read first value
            (32w0,    _, packet_type_t.CONSUME0) : write_read0_action();
            // If bitmap_before is nonzero, map_result is all 0's, and type is CONSUME0,
            // compute max of values and read first value
            (   _, 32w0, packet_type_t.CONSUME0) : max_read0_action();
            // If bitmap_before is nonzero, map_result is nonzero, and type is CONSUME0,
            // this is a retransmission, so just read first value
            (   _,    _, packet_type_t.CONSUME0) : read0_action();
            // if type is HARVEST7, read second value
            (   _,    _, packet_type_t.HARVEST7) : read1_action();
        }
        // If none of the above are true, do nothing.
        const default_action = NoAction;
    }

    apply {
        exponent_max.apply();
    }
}

#endif /* _EXPONENTS_ */
