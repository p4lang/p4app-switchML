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

/**
 * SwitchML Project
 * @file rdma_endpoint.cc
 * @brief Implements the RdmaEndpoint class
 */

#include "rdma_endpoint.h"

#include <sys/mman.h>
#include <rdma_utils.h>
#include <x86intrin.h>

#include <chrono>
#include <cstring>
#include <thread>

extern "C" {
#include <hugetlbfs.h>
}

#include "common_cc.h"

namespace switchml {

RdmaEndpoint::RdmaEndpoint(std::string device_name, uint16_t device_port_id, uint16_t gid_index)
        : device_(nullptr),
        device_port_id_(device_port_id), // port is generally 1-indexed
        port_attributes_(), // clear later
        gid_index_(gid_index), // use RoCEv2 with IP-based GID
        gid_(),
        context_(nullptr),
        protection_domain_(nullptr),
        ticks_per_sec_(0) {
    
    // Initialize GID
    gid_.global = {0, 0};

    // List of Verbs-capable devices
    ibv_device** devices;
    int num_devices;

    // Get device list
    devices = ibv_get_device_list(&num_devices);
    CHECK(devices) << "No Verbs-capable devices!";

    // Search for device

    for (int i = 0; i < num_devices; ++i) {
        VLOG(1) << "Found Verbs device " << ibv_get_device_name(devices[i])
                << " with guid "
                << (void*)be64toh(ibv_get_device_guid(devices[i]));

        if ((num_devices == 1) ||
            (device_name == ibv_get_device_name(devices[i]))) {
            // Choose this device
            this->device_ = devices[i];
        }
    }

    // Ensure we found a device
    CHECK(this->device_) << "Didn't find device " << device_name;

    VLOG(1) << "Using Verbs device " << ibv_get_device_name(this->device_)
                << " gid index " << this->gid_index_;

    // Open device context and get device attributes
    this->context_ = ibv_open_device(this->device_);
    CHECK(this->context_) << "Failed to get context for device " << device_name;

    // Free list of devices.
    // We should free the device list only after we've openned the context to ensure
    // that the device struct contents remain valid after freeing the device list.
    ibv_free_device_list(devices);

    ibv_device_attr device_attributes;
    std::memset(&device_attributes, 0, sizeof(device_attributes));

    PCHECK(ibv_query_device(this->context_, &device_attributes) >= 0)
        << "Error getting device attributes";

    // Choose a port on the device and get port attributes
    CHECK_GE(device_attributes.phys_port_cnt, this->device_port_id_)
        << "Expected at least " << this->device_port_id_ << " ports, but found "
        << device_attributes.phys_port_cnt;

    if (device_attributes.phys_port_cnt > 1) {
        VLOG(0) << device_attributes.phys_port_cnt << " ports detected; using port " << this->device_port_id_;
    }

    std::memset(&this->port_attributes_, 0, sizeof(this->port_attributes_));
    PCHECK(ibv_query_port(this->context_, this->device_port_id_, &this->port_attributes_) >= 0)
        << "Error getting port attributes";

    // Log GIDs
    for (int i = 0; i < this->port_attributes_.gid_tbl_len; ++i) {
        PCHECK(ibv_query_gid(this->context_, this->device_port_id_, i, &this->gid_) >= 0)
            << "Error getting GID";

        if (this->gid_.global.subnet_prefix != 0 || this->gid_.global.interface_id != 0) {
        VLOG(0) << "GID " << i << " is " << (void*) this->gid_.global.subnet_prefix
                    << " " << (void*) this->gid_.global.interface_id;
        }
    }

    // Get selected gid
    PCHECK(ibv_query_gid(this->context_, this->device_port_id_, this->gid_index_, &this->gid_) >= 0)
        << "Error getting GID";

    CHECK(this->gid_.global.subnet_prefix != 0 || this->gid_.global.interface_id != 0)
        << "Selected GID " << this->gid_index_
        << " was all zeros; is interface down? Maybe try RoCEv1 GID index?";

    // Create protection domain
    this->protection_domain_ = ibv_alloc_pd(this->context_);
    CHECK(this->protection_domain_) << "Error getting protection domain!";

    // Until we can use NIC timestamps, store the CPU timestamp counter tick rate
    uint64_t start_ticks = __rdtsc();
    std::this_thread::sleep_for(std::chrono::seconds(1));
    uint64_t end_ticks = __rdtsc();
    this->ticks_per_sec_ = end_ticks - start_ticks;
}

RdmaEndpoint::~RdmaEndpoint() {
    if (this->protection_domain_) {
        PCHECK(ibv_dealloc_pd(this->protection_domain_) >= 0)
            << "Error deallocating protection domain";
        this->protection_domain_ = nullptr;
    }

    if (this->context_) {
        PCHECK(ibv_close_device(this->context_) >= 0) << "Error closing device context";
        this->context_ = nullptr;
    }
}

ibv_mr* RdmaEndpoint::AllocateAtAddress(void* requested_address, size_t size) {
    // Round up to a multiple of huge page size
    size_t hugepagesize = gethugepagesize();
    CHECK_GE(hugepagesize, 0) << "Error getting default huge page size";
    size = (size + (hugepagesize - 1)) & ~(hugepagesize - 1);

    // Allocate
    void* buf =
        mmap(requested_address, size, PROT_READ | PROT_WRITE,
             MAP_PRIVATE | MAP_ANONYMOUS | MAP_HUGETLB | MAP_FIXED, -1, 0);
    PCHECK(buf != MAP_FAILED && buf == requested_address)
        << "Error allocating memory region";

    // Register memory region
    ibv_mr* mr = ibv_reg_mr(this->protection_domain_, buf, size,
                            (IBV_ACCESS_LOCAL_WRITE | IBV_ACCESS_REMOTE_WRITE |
                             IBV_ACCESS_ZERO_BASED));
    PCHECK(mr) << "Error registering memory region";

    return mr;
}

void RdmaEndpoint::free(ibv_mr* mr) {
    // Extract pointer from memory region
    auto buf = mr->addr;
    auto len = mr->length;

    // Deregister memory region
    PCHECK(ibv_dereg_mr(mr) >= 0) << "Error deregistering memory region";

    // Free memory region
    PCHECK(munmap(buf, len) >= 0) << "Error freeing memory region";
}

ibv_cq* RdmaEndpoint::CreateCompletionQueue() {
    return ibv_create_cq(context_, kCompletionQueueDepth,
                         NULL,  // no user context
                         NULL,  // no completion channel
                         0);    // no completion channel vector
}


ibv_qp* RdmaEndpoint::CreateQueuePair(ibv_cq* completion_queue) {
    // Create queue pair (starts in RESET state)
    ibv_qp_init_attr init_attributes;
    std::memset(&init_attributes, 0, sizeof(init_attributes));

    // Use one shared completion queue for each thread
    init_attributes.send_cq = completion_queue;
    init_attributes.recv_cq = completion_queue;

    // Use UC queue pair
    init_attributes.qp_type = IBV_QPT_UC;

    // Only issue send completions if requested
    init_attributes.sq_sig_all = 0;

    // Set queue depths and WR parameters
    init_attributes.cap.max_send_wr = kSendQueueDepth;
    init_attributes.cap.max_recv_wr = kReceiveQueueDepth;
    init_attributes.cap.max_send_sge = kScatterGatherElementCount;
    init_attributes.cap.max_recv_sge = kScatterGatherElementCount;
    init_attributes.cap.max_inline_data = kMaxInlineData;

    // Create queue pair
    ibv_qp* queue_pair = ibv_create_qp(this->protection_domain_, &init_attributes);
    CHECK(queue_pair) << "Error creating queue pair";

    return queue_pair;
}

uint64_t RdmaEndpoint::GetMac() {
    ibv_gid mac_gid;
    PCHECK(ibv_query_gid(this->context_, device_port_id_, 0, &mac_gid) >= 0)
        << "Error getting GID for MAC address";

    return GIDToMAC(mac_gid);
}

uint32_t RdmaEndpoint::GetIPv4() {
    ibv_gid ipv4_gid;
    PCHECK(ibv_query_gid(context_, device_port_id_, 2, &ipv4_gid) >= 0)
        << "Error getting GID for IPv4 address";

    return GIDToIPv4(ipv4_gid);
}

ibv_port_attr RdmaEndpoint::GetPortAttributes() { return this->port_attributes_; }

ibv_device* RdmaEndpoint::GetDevice() { return this->device_; }

} // namespace switchml