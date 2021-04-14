#ifndef _WORKERS_COUNTER_
#define _WORKERS_COUNTER_

control WorkersCounter(
    in header_t hdr,
    inout ingress_metadata_t ig_md,
    in ingress_intrinsic_metadata_for_deparser_t ig_dprsr_md) {

    Register<num_workers_pair_t, pool_index_t>(register_size) worker_count;

    RegisterAction<num_workers_pair_t, pool_index_t, num_workers_t>(worker_count) worker_count_action = {
        // 1 means last packet; 0 means first packet
        void apply(inout num_workers_pair_t value, out num_workers_t read_value) {
            // Only works with jobs of 2 workers or more
            read_value = value.first;

            if (value.first == 0) {
                value.first = ig_md.switchml_md.num_workers - 1;
            } else {
                value.first = value.first - 1;
            }
        }
    };

    RegisterAction<num_workers_pair_t, pool_index_t, num_workers_t>(worker_count) read_worker_count_action = {
        void apply(inout num_workers_pair_t value, out num_workers_t read_value) {
            read_value = value.first;
        }
    };

    action count_workers_action() {
        ig_md.switchml_md.first_last_flag = worker_count_action.execute(ig_md.switchml_md.pool_index);
    }

    action single_worker_count_action() {
        // Execute register action even though it's irrelevant with a single worker
        worker_count_action.execute(ig_md.switchml_md.pool_index);
        // Called for a new packet in a single worker job, so mark as last packet
        ig_md.switchml_md.first_last_flag = 1;
    }

    action single_worker_read_action() {
        // Called for a retransmitted packet in a single-worker job
        ig_md.switchml_md.first_last_flag = 0;
    }

    action read_count_workers_action() {
        ig_md.switchml_md.first_last_flag = read_worker_count_action.execute(ig_md.switchml_md.pool_index);
    }

    // If no bits are set in the map result, this was the first time we
    // saw this packet, so decrement worker count. Otherwise, it's a
    // retransmission, so just read the worker count.
    // Only act if packet type is CONSUME0
    table count_workers {
        key = {
            ig_md.switchml_md.num_workers: ternary;
            ig_md.switchml_md.map_result : ternary;
            ig_md.switchml_md.packet_type: ternary;
        }
        actions = {
            single_worker_count_action;
            single_worker_read_action;
            count_workers_action;
            read_count_workers_action;
            @defaultonly NoAction;
        }
        const entries = {
            // Special case for single-worker jobs
            // if map_result is all 0's and type is CONSUME0, this is the first time we've seen this packet
            (1, 0, packet_type_t.CONSUME0) : single_worker_count_action();
            (1, 0, packet_type_t.CONSUME1) : single_worker_count_action();
            (1, 0, packet_type_t.CONSUME2) : single_worker_count_action();
            (1, 0, packet_type_t.CONSUME3) : single_worker_count_action();

            // if we've seen this packet before, don't count, just read
            (1, _, packet_type_t.CONSUME0) : single_worker_read_action();
            (1, _, packet_type_t.CONSUME1) : single_worker_read_action();
            (1, _, packet_type_t.CONSUME2) : single_worker_read_action();
            (1, _, packet_type_t.CONSUME3) : single_worker_read_action();

            // Multi-worker jobs
            // if map_result is all 0's and type is CONSUME0, this is the first time we've seen this packet
            (_, 0, packet_type_t.CONSUME0) : count_workers_action();
            (_, 0, packet_type_t.CONSUME1) : count_workers_action();
            (_, 0, packet_type_t.CONSUME2) : count_workers_action();
            (_, 0, packet_type_t.CONSUME3) : count_workers_action();
            // if map_result is not all 0's and type is CONSUME0, don't count, just read
            (_, _, packet_type_t.CONSUME0) : read_count_workers_action();
            (_, _, packet_type_t.CONSUME1) : read_count_workers_action();
            (_, _, packet_type_t.CONSUME2) : read_count_workers_action();
            (_, _, packet_type_t.CONSUME3) : read_count_workers_action();
        }
        const default_action = NoAction;
    }

    apply {
        count_workers.apply();
    }
}

#endif /* _WORKERS_COUNTER_ */
