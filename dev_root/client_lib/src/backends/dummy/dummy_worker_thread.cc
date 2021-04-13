/**
 * SwitchML Project
 * @file dummy_worker_thread.cc
 * @brief Implements the DummyWorkerThread class.
 */

#include "dummy_worker_thread.h"

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>

#include <algorithm>

#include "common_cc.h"
#include "context.h"
#include "backend.h"

namespace switchml {

WorkerTid DummyWorkerThread::next_tid_ = 0;

DummyWorkerThread::DummyWorkerThread(Context& context, DummyBackend& backend, Config& config) :
tid_(DummyWorkerThread::next_tid_++),
context_(context),
backend_(backend),
config_(config),
thread_(nullptr),
ppp_()
{
    this->ppp_ = PrePostProcessor::CreateInstance(config, this->tid_);
}

DummyWorkerThread::~DummyWorkerThread(){
    // Do nothing
}

void DummyWorkerThread::operator()() {
    VLOG(0) << "Worker thread '" << this->tid_ << "' starting.";

    Context& ctx = this->context_;
    DummyBackend& backend = this->backend_;
    const GeneralConfig& genconf = this->config_.general_;
    // The maximum number of outstanding packets for this worker.
    const uint64_t max_outstanding_pkts = genconf.max_outstanding_packets/genconf.num_worker_threads;
    backend.SetupWorkerThread(this->tid_);

    // Buffers to hold the data that's supposed to be outstanding.
    void* outstanding_entries = malloc(max_outstanding_pkts*genconf.packet_numel*4); // Size of entry assumed to be 4 bytes at most
    void* outstanding_extra_info = malloc(max_outstanding_pkts*2); // 2 bytes extra info for each packet

    // The job slice struct that will be filled with the next job slice to work on.
    JobSlice job_slice;
    // Main worker thread loop
    while(ctx.GetContextState() == Context::ContextState::RUNNING) {
        // Get a job slice
        bool got_job_slice = ctx.GetJobSlice(this->tid_, job_slice);
        if(!got_job_slice) {
            continue;
        }
        DVLOG(2) << "Worker thread '" << this->tid_ << "' received job slice with job id: " << job_slice.job->id_ << " with numel: " << job_slice.slice.numel << ".";

        if(this->config_.general_.instant_job_completion) {
            if(ctx.GetContextState() == Context::ContextState::RUNNING) {
                DVLOG(2) << "Worker thread '" << this->tid_ << "' notifying job slice completion with job id: " << job_slice.job->id_  << ".";
                ctx.NotifyJobSliceCompletion(this->tid_, job_slice);
            }
            continue;
        }

        // Compute number of packets needed
        uint64_t total_num_pkts = (job_slice.slice.numel + genconf.packet_numel - 1) / genconf.packet_numel; // Roundup division

        // We can logically divide all of the packets that we will send into 'max_outstanding_pkts' sized groups
        // (Or less in case the total number of packets was less than max_outsanding_pkts).
        // We call each of these groups a batch. So if max_outstanding_pkts=10 and we wanted to send 70 packets then we have 7 batches.
        uint64_t batch_num_pkts = std::min(max_outstanding_pkts, total_num_pkts);

        this->ppp_->SetupJobSlice(&job_slice, total_num_pkts, batch_num_pkts);

        if(this->ppp_->NeedsExtraBatch()) {
            total_num_pkts += batch_num_pkts;
        }

        DVLOG(3) << "Worker thread '" << this->tid_ << "' will send a total of '" << total_num_pkts << "' packets each having '" << genconf.packet_numel << " elements.";

        // Create first batch of packets
        std::vector<DummyBackend::DummyPacket> first_batch_pkts;
        first_batch_pkts.reserve(batch_num_pkts);
        for(uint64_t i = 0, elements_finished = 0; i < batch_num_pkts; i++, elements_finished += genconf.packet_numel) {
            struct DummyBackend::DummyPacket pkt;
            pkt.pkt_id = i;
            pkt.job_id = job_slice.job->id_;
            pkt.numel = genconf.packet_numel;
            pkt.data_type = job_slice.slice.data_type;
            pkt.entries_ptr = (void*) (((uintptr_t) outstanding_entries) + (pkt.pkt_id % batch_num_pkts) * DataTypeSize(pkt.data_type) * genconf.packet_numel);
            pkt.extra_info_ptr = (void*) (((uintptr_t) outstanding_extra_info) + (pkt.pkt_id % batch_num_pkts) * 2);
            this->ppp_->PreprocessSingle(pkt.pkt_id, pkt.entries_ptr , pkt.extra_info_ptr);
            first_batch_pkts.push_back(pkt);
        }

        // Send first burst
        DVLOG(3) << "Worker thread '" << this->tid_ << "' will send the first '" << first_batch_pkts.size() << "' packets";
        backend.SendBurst(this->tid_, first_batch_pkts);
        ctx.GetStats().AddTotalPktsSent(this->tid_, first_batch_pkts.size());

        // loop until all packets have been sent and received.
        DVLOG(3) << "Worker thread '" << this->tid_ << "' is starting the receive and send loop";
        uint64_t num_packets_received = 0; 
        while(num_packets_received != total_num_pkts && ctx.GetContextState() == Context::ContextState::RUNNING) {
            // Receive group of packets
            std::vector<DummyBackend::DummyPacket> received_packets;
            backend.ReceiveBurst(this->tid_, received_packets);

            // To support both blocking calls and polling we check if we received any packets.
            if(received_packets.size() == 0) {
                continue;
            }
            ctx.GetStats().AddCorrectPktsReceived(this->tid_, received_packets.size());
            num_packets_received += received_packets.size();
            DVLOG(3) << "Worker thread '" << this->tid_ << "' received '" << received_packets.size()
                << "' packets. Total received '" << num_packets_received << "/" << total_num_pkts << "'.";

            // Create next group of packets to send including retransmissions.
            std::vector<DummyBackend::DummyPacket> packets_to_send;

            // Add new packets corresponding to received packets
            for(uint64_t i = 0; i < received_packets.size(); i++) {
                struct DummyBackend::DummyPacket pkt = received_packets.at(i);
                DVLOG(3) << "Worker thread '" << this->tid_ << "' retrieved packet '" << pkt.pkt_id << "'.";

                this->ppp_->PostprocessSingle(pkt.pkt_id, pkt.entries_ptr, pkt.extra_info_ptr);

                // What's the next pkt id if we were to reuse this packet?
                pkt.pkt_id += batch_num_pkts;

                // Do we need to reuse the packet?
                if(pkt.pkt_id >= total_num_pkts) {
                    continue;
                }
                DVLOG(3) << "Worker thread '" << this->tid_ << "' creating packet '" << pkt.pkt_id << "'.";

                // Compute pointers to the entries and extra info outstanding buffers
                pkt.entries_ptr = (void*) (((uintptr_t) outstanding_entries) + (pkt.pkt_id % batch_num_pkts) * DataTypeSize(pkt.data_type) * genconf.packet_numel);
                pkt.extra_info_ptr = (void*) (((uintptr_t) outstanding_extra_info) + (pkt.pkt_id % batch_num_pkts) * 2);

                this->ppp_->PreprocessSingle(pkt.pkt_id, pkt.entries_ptr , pkt.extra_info_ptr);

                packets_to_send.push_back(pkt);
            }

            if(packets_to_send.size() == 0) {
                continue;
            }

            // Send the next group of packets
            DVLOG(3) << "Worker thread '" << this->tid_ << "' sending '" << packets_to_send.size() << "' packets.";
            backend.SendBurst(this->tid_, packets_to_send);
            ctx.GetStats().AddTotalPktsSent(this->tid_, packets_to_send.size());

        } // while (num_packets_received != total_num_pkts && ctx.GetContextState() == Context::ContextState::RUNNING)

        this->ppp_->CleanupJobSlice();
        
        // Notify the ctx that the worker thread finished this job slice.
        if(ctx.GetContextState() == Context::ContextState::RUNNING) {
            DVLOG(2) << "Worker thread '" << this->tid_ << "' notifying job slice completion with job id: " << job_slice.job->id_  << ".";
            ctx.NotifyJobSliceCompletion(this->tid_, job_slice);
        }

    } // while(ctx.GetContextState() == Context::ContextState::RUNNING)

    VLOG(0) << "Worker thread '" << this->tid_ << "' exiting.";
    free(outstanding_entries);
    free(outstanding_extra_info);
    backend.CleanupWorkerThread(this->tid_);
}

void DummyWorkerThread::Start() {
    LOG_IF(FATAL, this->thread_ != nullptr) << "Trying to start a thread twice.";
    // SUGGESTION: This works but is it the best way to to store a reference in the same class?
    this->thread_ = new std::thread(*this); // This will create a new thread that calls the operator()() function.
}

void DummyWorkerThread::Join() {
    LOG_IF(FATAL, this->thread_ == nullptr) << "Trying to join a thread that hasn't started or has already been joined.";
    this->thread_->join();
    delete this->thread_;
}

} // namespace switchml