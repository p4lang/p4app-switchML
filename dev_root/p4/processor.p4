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

#ifndef _PROCESSOR_
#define _PROCESSOR_

#include "types.p4"
#include "headers.p4"

// Sum calculator
// Each control handles two values
control Processor(
    in value_t value0,
    in value_t value1,
    out value_t value0_out,
    out value_t value1_out,
    in switchml_md_h switchml_md) {

    Register<value_pair_t, pool_index_t>(register_size) values;

    // Write both values and read first one
    RegisterAction<value_pair_t, pool_index_t, value_t>(values) write_read1_register_action = {
        void apply(inout value_pair_t value, out value_t read_value) {
            value.first = value0;
            value.second = value1;
            read_value = value.second;
        }
    };

    action write_read1_action() {
        value1_out = write_read1_register_action.execute(switchml_md.pool_index);
    }

    // Compute sum of both values and read first one
    RegisterAction<value_pair_t, pool_index_t, value_t>(values) sum_read1_register_action = {
        void apply(inout value_pair_t value, out value_t read_value) {
            value.first  = value.first  + value0;
            value.second = value.second + value1;
            read_value = value.second;
        }
    };

    action sum_read1_action() {
        value1_out = sum_read1_register_action.execute(switchml_md.pool_index);
    }

    // Read first sum register
    RegisterAction<value_pair_t, pool_index_t, value_t>(values) read0_register_action = {
        void apply(inout value_pair_t value, out value_t read_value) {
            read_value = value.first;
        }
    };

    action read0_action() {
        value0_out = read0_register_action.execute(switchml_md.pool_index);
    }

    // Read second sum register
    RegisterAction<value_pair_t, pool_index_t, value_t>(values) read1_register_action = {
        void apply(inout value_pair_t value, out value_t read_value) {
            read_value = value.second;
        }
    };

    action read1_action() {
        value1_out = read1_register_action.execute(switchml_md.pool_index);
    }

    // If bitmap_before is 0 and type is CONSUME0, write values and read second value
    // If bitmap_before is not zero and type is CONSUME0, add values and read second value
    // If map_result is not zero and type is CONSUME0, just read first value
    // If type is HARVEST, read second value
    table sum {
        key = {
            switchml_md.worker_bitmap_before : ternary;
            switchml_md.map_result : ternary;
            switchml_md.packet_type: ternary;
        }
        actions = {
            write_read1_action;
            sum_read1_action;
            read0_action;
            read1_action;
            NoAction;
        }
        size = 20;
        const entries = {
            // If bitmap_before is all 0's and type is CONSUME0, this is the first packet for slot,
            // so just write values and read second value
            (32w0,    _, packet_type_t.CONSUME0) : write_read1_action();
            (32w0,    _, packet_type_t.CONSUME1) : write_read1_action();
            (32w0,    _, packet_type_t.CONSUME2) : write_read1_action();
            (32w0,    _, packet_type_t.CONSUME3) : write_read1_action();
            // If bitmap_before is nonzero, map_result is all 0's, and type is CONSUME0,
            // compute sum of values and read second value
            (   _, 32w0, packet_type_t.CONSUME0) : sum_read1_action();
            (   _, 32w0, packet_type_t.CONSUME1) : sum_read1_action();
            (   _, 32w0, packet_type_t.CONSUME2) : sum_read1_action();
            (   _, 32w0, packet_type_t.CONSUME3) : sum_read1_action();
            // If bitmap_before is nonzero, map_result is nonzero, and type is CONSUME0,
            // this is a retransmission, so just read first value
            (   _,    _, packet_type_t.CONSUME0) : read0_action();
            (   _,    _, packet_type_t.CONSUME1) : read0_action();
            (   _,    _, packet_type_t.CONSUME2) : read0_action();
            (   _,    _, packet_type_t.CONSUME3) : read0_action();
            // If type is HARVEST, read one set of values based on sequence
            (   _,    _, packet_type_t.HARVEST0) : read1_action(); // extract data1 slice in pipe 3
            (   _,    _, packet_type_t.HARVEST1) : read0_action(); // extract data0 slice in pipe 3
            (   _,    _, packet_type_t.HARVEST2) : read1_action(); // extract data1 slice in pipe 2
            (   _,    _, packet_type_t.HARVEST3) : read0_action(); // extract data0 slice in pipe 2
            (   _,    _, packet_type_t.HARVEST4) : read1_action(); // extract data1 slice in pipe 1
            (   _,    _, packet_type_t.HARVEST5) : read0_action(); // extract data0 slice in pipe 1
            (   _,    _, packet_type_t.HARVEST6) : read1_action(); // extract data1 slice in pipe 0
            (   _,    _, packet_type_t.HARVEST7) : read0_action(); // last pass; extract data0 slice in pipe 0
        }
        // if none of the above are true, do nothing.
        const default_action = NoAction;
    }

    apply {
        sum.apply();
    }
}

#endif /* _PROCESSOR_ */
