#ifndef _CONFIGURATION_
#define _CONFIGURATION_

// Register size
// 16384 is the largest power-of-two stateful 64b register size per stage in Tofino 1
// This is enough for a single 2MB message in flight when using 2 slots
const int register_size = 16384;

// Each slot has two registers because of the shadow copy
const int num_slots = register_size / 2;

// Max number of SwitchML workers we support
const int max_num_workers = 32;
const int max_num_workers_log2 = 5; // log base 2 of max_num_workers

// Size of the forwarding table
const int forwarding_table_size = 1024;

// Number of destination queue pairs per-worker
const int max_num_queue_pairs_per_worker = 512;
const int max_num_queue_pairs_per_worker_log2 = 9;

// Total number of destination queue pairs
const int max_num_queue_pairs = max_num_queue_pairs_per_worker * max_num_workers;
const int max_num_queue_pairs_log2 = max_num_queue_pairs_per_worker_log2 + max_num_workers_log2;

// Exclusion ID value to use when we don't want to exclude any nodes
// during multicast
const bit<16> null_level1_exclusion_id = 0xffff;

#endif /* _CONFIGURATION_ */
