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

namespace switchml {

DummyBackend::DummyBackend(Context& context, Config& config)
    : Backend(context, config)
    , worker_threads_()
    , pending_packets_()
{
    uint16_t num_worker_threads = this->config_.general_.num_worker_threads;
    // SUGGESTION: Use smart pointers or vectors instead?
    this->worker_threads_ = new std::thread[num_worker_threads];
    this->pending_packets_ = new std::vector<DummyPacket>[num_worker_threads];
}

DummyBackend::~DummyBackend() {
    delete [] this->worker_threads_;
    delete [] this->pending_packets_;
}

void DummyBackend::SetupWorker() {
    VLOG(0) << "Setting up worker.";
    for(int i = 0; i < this->config_.general_.num_worker_threads; i++) {
        this->worker_threads_[i] = std::thread(DummyWorkerThread(this->context_,*this, this->config_));
    }
}

void DummyBackend::CleanupWorker() {
    VLOG(0) << "Cleaning up worker.";
    for(int i = 0; i < this->config_.general_.num_worker_threads; i++) {
        this->worker_threads_[i].join();
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
    if(pkt.tensor.data_type == FLOAT32) {
        float* in_data = static_cast<float*>(pkt.tensor.in_ptr);
        float* out_data = static_cast<float*>(pkt.tensor.out_ptr);
        for(Numel j = 0; j < pkt.tensor.numel; j++) {
            out_data[j] = in_data[j] * this->config_.general_.num_workers;
        }
    } else if(pkt.tensor.data_type == INT32) {
        int32_t* in_data = static_cast<int32_t*>(pkt.tensor.in_ptr);
        int32_t* out_data = static_cast<int32_t*>(pkt.tensor.out_ptr);
        for(Numel j = 0; j < pkt.tensor.numel; j++) {
            out_data[j] = in_data[j] * this->config_.general_.num_workers;
        }
    }
}

void DummyBackend::SendBurst(WorkerTid worker_thread_id, const std::vector<DummyPacket>& packets_to_send) {
    CHECK(packets_to_send.size() > 0) << "Worker thread '" << worker_thread_id << "' trying to send 0 packets.";

    for(size_t i = 0; i < packets_to_send.size(); i++) {
        struct DummyPacket pkt = packets_to_send.at(i);
        DVLOG(3) << "Worker thread '" << worker_thread_id << "' sending pkt '" << pkt.packet_index << " with size '"
            << pkt.tensor.numel * DataTypeSize(pkt.tensor.data_type) << "' bytes.";
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

        bytes_received += pkt.tensor.numel * DataTypeSize(pkt.tensor.data_type);
        DVLOG(3) << "Worker thread '" << worker_thread_id << "' receiving pkt '" << pkt.packet_index
            << " with size '" << pkt.tensor.numel * DataTypeSize(pkt.tensor.data_type) << "' bytes.";

        // Add to received and remove from pending
        packets_received.push_back(pkt);
        worker_thread_pending_packet.erase(worker_thread_pending_packet.begin() + i);
    }
    // Sleep to simulate network and switch latency
    struct timespec req = {0, 0};
    // We multiply by the number of worker threads because hypothetically they will all be contending for the link
    // So this is a decent approximation of the real thing.
    if (this->config_.backend_.dummy.bandwidth > 0) {
        req.tv_nsec = 1000 * bytes_received * 8 * this->config_.general_.num_worker_threads / this->config_.backend_.dummy.bandwidth;
        DVLOG(3) << "Worker thread '" << worker_thread_id << "' received '" << nr << "' packets with total size '" << bytes_received << "' bytes. " 
            << "Sleeping for " << req.tv_nsec << "' ns.";
        nanosleep(&req, (struct timespec*) NULL);
    }
}

} // namespace switchml