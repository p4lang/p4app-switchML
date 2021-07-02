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
 * @file dummy_backend.cc
 * @brief Implements the DummyBackend class.
 */

#include "dummy_backend.h"

#include <stdlib.h>

#include <thread>

#include "common_cc.h"
#include "dummy_worker_thread.h"

#define SWAP_INT32(a) ((a >> 24) & 0xFF) | ((a << 8) & 0xFF0000) | ((a >> 8) & 0xFF00) | ((a << 24) & 0xFF000000)

namespace switchml {

DummyBackend::DummyBackend(Context& context, Config& config)
    : Backend(context, config)
    , worker_threads_()
    , pending_packets_()
{
    uint16_t num_worker_threads = this->config_.general_.num_worker_threads;
    this->pending_packets_ = new std::vector<DummyPacket>[num_worker_threads];
}

DummyBackend::~DummyBackend() {
    delete [] this->pending_packets_;
}

void DummyBackend::SetupWorker() {
    VLOG(0) << "Setting up worker.";
    for(int i = 0; i < this->config_.general_.num_worker_threads; i++) {
        this->worker_threads_.push_back(DummyWorkerThread(this->context_,*this, this->config_));
        this->worker_threads_[i].Start();
    }
}

void DummyBackend::CleanupWorker() {
    VLOG(0) << "Cleaning up worker.";
    for(int i = 0; i < this->config_.general_.num_worker_threads; i++) {
        this->worker_threads_[i].Join();
    }
}

void DummyBackend::SetupWorkerThread(WorkerTid worker_thread_id) {
    VLOG(0) << "Setting up worker thread '" << worker_thread_id << "'.";
}

void DummyBackend::CleanupWorkerThread(WorkerTid worker_thread_id) {
    VLOG(0) << "Cleaning up worker thread '" << worker_thread_id << "'.";
}

void DummyBackend::ProcessPacket(DummyPacket& pkt) {
    // Multiply all elements by the number of workers to simulate all reduce.
    // We assume all entries received are big endian int32 (As would be the case with the switch)
    int32_t* entries_ptr = static_cast<int32_t*>(pkt.entries_ptr);
    DVLOG(4) << "Processing packet '" << pkt.pkt_id << "'.";
    for(Numel j = 0; j < pkt.numel; j++) {
        DVLOG(4) << "Before entries_ptr[" << j << "]=" << entries_ptr[j];
        entries_ptr[j] = SWAP_INT32(entries_ptr[j]);
        entries_ptr[j] *= this->config_.general_.num_workers;
        entries_ptr[j] = SWAP_INT32(entries_ptr[j]);
        DVLOG(4) << "After entries_ptr[" << j << "]=" << entries_ptr[j];
    }
}

void DummyBackend::SendBurst(WorkerTid worker_thread_id, const std::vector<DummyPacket>& packets_to_send) {
    CHECK(packets_to_send.size() > 0) << "Worker thread '" << worker_thread_id << "' trying to send 0 packets.";

    for(size_t i = 0; i < packets_to_send.size(); i++) {
        struct DummyPacket pkt = packets_to_send.at(i);
        DVLOG(3) << "Worker thread '" << worker_thread_id << "' sending pkt '" << pkt.pkt_id << " with size '"
            << pkt.numel * DataTypeSize(pkt.data_type) << "' bytes.";
        this->pending_packets_[worker_thread_id].push_back(pkt);
    }
}

void DummyBackend::ReceiveBurst(WorkerTid worker_thread_id, std::vector<DummyPacket>& packets_received) {
    std::vector<DummyPacket>& worker_thread_pending_packet = this->pending_packets_[worker_thread_id];
    CHECK(worker_thread_pending_packet.size() > 0) << "Worker thread '" << worker_thread_id
        << "' trying to receive after all packets have been received.";

    // Choose how many packets will we receive
    int num_receives = std::rand() % (worker_thread_pending_packet.size()+1);
    int nr = num_receives;
    uint64_t bytes_received = 0;
    while(num_receives--) {
        // Choose packet index
        int i = std::rand() % worker_thread_pending_packet.size();
        struct DummyPacket pkt = worker_thread_pending_packet.at(i);

        // Process packet
        if (this->config_.backend_.dummy.process_packets) {
            this->ProcessPacket(pkt);
        }

        bytes_received += pkt.numel * DataTypeSize(pkt.data_type);
        DVLOG(3) << "Worker thread '" << worker_thread_id << "' receiving pkt '" << pkt.pkt_id
            << " with size '" << pkt.numel * DataTypeSize(pkt.data_type) << "' bytes.";

        // Add to received and remove from pending
        packets_received.push_back(pkt);
        worker_thread_pending_packet.erase(worker_thread_pending_packet.begin() + i);
    }
    // Sleep to simulate network and switch latency
    struct timespec req = {0, 0};
    // We multiply by the number of worker threads because realistically they will all be contending for the link
    // So this is a better approximation of the real thing.
    if (bytes_received > 0 && this->config_.backend_.dummy.bandwidth > 0) {
        req.tv_nsec = 1000 * bytes_received * 8 * this->config_.general_.num_worker_threads / this->config_.backend_.dummy.bandwidth;
        DVLOG(3) << "Worker thread '" << worker_thread_id << "' received '" << nr << "' packets with total size '" << bytes_received << "' bytes. " 
            << "Sleeping for " << req.tv_nsec << "' ns.";
        nanosleep(&req, (struct timespec*) NULL);
    }
}

} // namespace switchml