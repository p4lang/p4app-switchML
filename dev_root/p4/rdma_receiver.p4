#ifndef _RDMA_RECEIVER_
#define _RDMA_RECEIVER_

// Switch QPN format:
// - bits [15:0] are the queue pair index for this worker. 
// - higher bits are the worker ID. Since we only support 32
//   workers right now, only bits 20 down to 16 should ever be used.
// Since each RDMA message in flight requires its own queue pair, and the
// minimum message size is the size of a slot, the most queue pairs we
// could ever need is the number of slots. The max number of slots we
// can have is 11264 (22528/2), so this actually only needs 14 bits.

// Compute queue pair index
#define QP_INDEX (hdr.ib_bth.dst_qp[16+max_num_workers_log2-1:16] ++ hdr.ib_bth.dst_qp[max_num_queue_pairs_per_worker_log2-1:0])

// We take the pool index directly from the rkey
#define POOL_INDEX (hdr.ib_reth.r_key)

typedef bit<32> return_t;

struct receiver_data_t {
    bit<32> next_sequence_number;
    bit<32> pool_index;
}

control RDMAReceiver(
    inout header_t hdr,
    inout ingress_metadata_t ig_md,
    in ingress_intrinsic_metadata_t ig_intr_md,
    in ingress_intrinsic_metadata_from_parser_t ig_prsr_md,
    inout ingress_intrinsic_metadata_for_deparser_t ig_dprsr_md,
    inout ingress_intrinsic_metadata_for_tm_t ig_tm_md) {

    DirectCounter<counter_t>(CounterType_t.PACKETS_AND_BYTES) rdma_receive_counter;

    Register<receiver_data_t, queue_pair_index_t>(max_num_queue_pairs) receiver_data_register;
    Counter<counter_t, queue_pair_index_t>(max_num_queue_pairs, CounterType_t.PACKETS) rdma_packet_counter;
    Counter<counter_t, queue_pair_index_t>(max_num_queue_pairs, CounterType_t.PACKETS) rdma_message_counter;
    Counter<counter_t, queue_pair_index_t>(max_num_queue_pairs, CounterType_t.PACKETS) rdma_sequence_violation_counter;

    bool message_possibly_received;
    bool sequence_violation;

    // Use with _FIRST and _ONLY packets
    RegisterAction<receiver_data_t, queue_pair_index_t, return_t>(receiver_data_register) receiver_reset_action = {
        void apply(inout receiver_data_t value, out return_t read_value) {
            // Store next sequence number.
            if ((bit<32>) hdr.ib_bth.psn == 0x00ffffff) { // PSNs are 24 bits. Do we need to wrap around?
                // Wrap around
                value.next_sequence_number = 0;
            } else {
                // Increment and store next sequence number
                value.next_sequence_number = (bit<32>) hdr.ib_bth.psn + 1;
            }

            // Reset pool index register using pool index in packet.
            value.pool_index = (bit<32>) POOL_INDEX;

            // Return pool index field. MSB is error bit, LSBs are the actual pool index.
            read_value = value.pool_index;
        }
    };

    // Use with _MIDDLE and _LAST packets
    RegisterAction<receiver_data_t, queue_pair_index_t, return_t>(receiver_data_register) receiver_increment_action = {
        void apply(inout receiver_data_t value, out return_t read_value) {
            // is this packet the next one in the PSN sequence?
            if ((bit<32>) hdr.ib_bth.psn == value.next_sequence_number) {
                // Yes. Increment PSN and pool index

                // Increment pool index, leaving slot bit (bit 0) unchanged.
                // Use saturating addition to ensure error bit stays set if it gets set
                value.pool_index = value.pool_index |+| 2;

                // Increment expected sequence number
                if (value.next_sequence_number == 0x00ffffff) { // PSNs are 24 bits. Do we need to wrap around?
                    // Wrap around
                    value.next_sequence_number = 0;
                } else {
                    // Increment PSN
                    value.next_sequence_number = value.next_sequence_number + 1;
                }
            } else {
                // No! Leave next PSN unchanged and signal error using MSB of pool index

                // Set error bit in pool index register. Once this bit
                // is set, it stays set until a new message arrives
                // and overwrites the register state.
                value.pool_index = 0x80000000;

                // The error bit will cause packets to be dropped even
                // if the next packet in the sequence eventually
                // arrives.
                value.next_sequence_number = value.next_sequence_number;
            }

            // Return pool index field. MSB is error bit, LSBs are the actual pool index.
            read_value = value.pool_index;
        }
    };

    action set_bitmap(
        MulticastGroupId_t mgid,
        worker_type_t worker_type,
        worker_id_t worker_id,
        num_workers_t num_workers,
        worker_bitmap_t worker_bitmap,
        packet_size_t packet_size,
        drop_probability_t drop_probability) {

        // Bitmap representation for this worker
        ig_md.worker_bitmap   = worker_bitmap;
        ig_md.switchml_md.num_workers     = num_workers;

        // Group ID for this job
        ig_md.switchml_md.mgid = mgid;

        ig_md.switchml_md.worker_type = worker_type;
        ig_md.switchml_md.worker_id = worker_id; // Same as rid for worker; used when retransmitting RDMA packets
        ig_md.switchml_md.dst_port = hdr.udp.src_port;

        ig_md.switchml_rdma_md.rdma_addr = hdr.ib_reth.addr;
        ig_md.switchml_md.tsi = hdr.ib_reth.len;

        // Get rid of headers we don't want to recirculate
        hdr.ethernet.setInvalid();
        hdr.ipv4.setInvalid();
        hdr.udp.setInvalid();
        hdr.ib_bth.setInvalid();
        hdr.ib_reth.setInvalid();
        hdr.ib_immediate.setInvalid();

        // Record packet size for use in recirculation
        ig_md.switchml_md.packet_size = packet_size;
        ig_md.switchml_md.recirc_port_selector = (queue_pair_index_t) QP_INDEX;

        rdma_receive_counter.count();
    }

    action assign_pool_index(bit<32> result) {
        ig_md.switchml_md.pool_index = result[14:0];
    }

    action middle_packet(
        MulticastGroupId_t mgid,
        worker_type_t worker_type,
        worker_id_t worker_id,
        num_workers_t num_workers,
        worker_bitmap_t worker_bitmap,
        packet_size_t packet_size,
        drop_probability_t drop_probability) {

        // Set common fields
        set_bitmap(mgid, worker_type, worker_id,  num_workers, worker_bitmap, packet_size, drop_probability);
        ig_md.switchml_rdma_md.first_packet = false;
        ig_md.switchml_rdma_md.last_packet  = false;

        // Process sequence number
        return_t result = receiver_increment_action.execute(QP_INDEX);

        // If sign bit of result is set, there was a sequence violation, so ignore this packet
        ig_dprsr_md.drop_ctl[0:0] = result[31:31];

        assign_pool_index(result);

        // A message has not yet arrived, since this is a middle packet
        message_possibly_received = false;
        sequence_violation = (bool) result[31:31];
    }

    action last_packet(
        MulticastGroupId_t mgid,
        worker_type_t worker_type,
        worker_id_t worker_id,
        num_workers_t num_workers,
        worker_bitmap_t worker_bitmap,
        packet_size_t packet_size,
        drop_probability_t drop_probability) {

        // Set common fields
        set_bitmap(mgid, worker_type, worker_id,  num_workers, worker_bitmap, packet_size, drop_probability);
        ig_md.switchml_rdma_md.first_packet = false;
        ig_md.switchml_rdma_md.last_packet  = true;

        // Process sequence number
        return_t result = receiver_increment_action.execute(QP_INDEX);

        // If sign bit of result is set, there was a sequence violation, so ignore this packet
        ig_dprsr_md.drop_ctl[0:0] = result[31:31];

        assign_pool_index(result);

        // A message has arrived if the sequence number matched (sign bit == 0)
        message_possibly_received = true;
        sequence_violation = (bool) result[31:31];
    }

    action first_packet(
        MulticastGroupId_t mgid,
        worker_type_t worker_type,
        worker_id_t worker_id,
        num_workers_t num_workers,
        worker_bitmap_t worker_bitmap,
        packet_size_t packet_size,
        drop_probability_t drop_probability) {

        // Set common fields
        set_bitmap(mgid, worker_type, worker_id,  num_workers, worker_bitmap, packet_size, drop_probability);
        ig_md.switchml_rdma_md.first_packet = true;
        ig_md.switchml_rdma_md.last_packet  = false;

        // Reset sequence number
        return_t result = receiver_reset_action.execute(QP_INDEX);

        assign_pool_index(result);

        // This is a first packet, so there can be no sequence number violation
        // Don't drop the packet.

        // A message has not yet arrived, since this is a first packet
        message_possibly_received = false;
        sequence_violation = (bool) result[31:31];
    }

    action only_packet(
        MulticastGroupId_t mgid,
        worker_type_t worker_type,
        worker_id_t worker_id,
        num_workers_t num_workers,
        worker_bitmap_t worker_bitmap,
        packet_size_t packet_size,
        drop_probability_t drop_probability) {

        // Set common fields
        set_bitmap(mgid, worker_type, worker_id,  num_workers, worker_bitmap, packet_size, drop_probability);
        ig_md.switchml_rdma_md.first_packet = true;
        ig_md.switchml_rdma_md.last_packet  = true;

        // Reset sequence number
        return_t result = receiver_reset_action.execute(QP_INDEX);

        assign_pool_index(result);

        // This is an only packet, so there can be no sequence number violation.
        // Don't drop the packet

        // A message has arrived, since this is an only packet
        message_possibly_received = true;
        sequence_violation = (bool) result[31:31];
    }

    // This is a regular packet; just forward
    action forward() {
        ig_md.switchml_md.packet_type = packet_type_t.IGNORE;
        rdma_receive_counter.count();
    }


    // The goal for this component is to receive a RoCEv2 RDMA UC packet
    // stream, and to compute the correct slot ID for each packet.
    //
    // Since we can only provide the slot ID in the first packet of
    // the message (in the RDMA address header), we have to cache it
    // in the switch, and compute it based on the cached value in each
    // subsequent packet.
    //
    // It's important to avoid screwing up this computation if packets
    // are lost or reordered. The safest way to do this is to require
    // packets to be processed in order, and discarded if they
    // aren't. Since we have bitmaps protecting us from adding a
    // particular packet's contribution multiple times, if a
    // particular message has some packets received and added and some
    // not and the message is retransmitted, the packets that have
    // already been received will be ignored.
    table receive_roce {
        key = {
            hdr.ipv4.src_addr        : exact;
            hdr.ipv4.dst_addr        : exact;
            hdr.ib_bth.partition_key : exact;
            hdr.ib_bth.opcode        : exact;
            hdr.ib_bth.dst_qp        : ternary; // match on top 8 bits
        }
        actions = {
            only_packet;
            first_packet;
            middle_packet;
            last_packet;
            @defaultonly forward;
        }
        const default_action = forward;
        size = max_num_workers * 6; // each worker needs 6 entries: first, middle, last, only, last immediate, and only immediate
        counters = rdma_receive_counter;
    }

    apply {
        if (receive_roce.apply().hit) {

            // Count received packets for this queue pair
            rdma_packet_counter.count(ig_md.switchml_md.recirc_port_selector);

            if (sequence_violation) {
                // Count sequence violation, drop bit is already set
                rdma_sequence_violation_counter.count(ig_md.switchml_md.recirc_port_selector);

            } else if (message_possibly_received) {
                // Count correctly received message
                rdma_message_counter.count(ig_md.switchml_md.recirc_port_selector);
            }
        }
    }
}

#endif /* _RDMA_RECEIVER_ */
