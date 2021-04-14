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
config_(config)
{
    // Do nothing
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
    const uint64_t outstanding_pkts = genconf.max_outstanding_packets/genconf.num_worker_threads;
    backend.SetupWorkerThread(this->tid_);

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

        // TODO: Preprocess job slice

        // Compute number of packets needed
        uint64_t total_num_packets = (job_slice.slice.numel + genconf.packet_numel - 1) / genconf.packet_numel; // Roundup division
        DVLOG(3) << "Worker thread '" << this->tid_ << "' will send a total of '" << total_num_packets << "' packets each having a maximum of '" << genconf.packet_numel << " elements.";

        // Create first burst of packets
        uint64_t first_burst_num_packets = std::min(outstanding_pkts, total_num_packets);
        std::vector<DummyBackend::DummyPacket> first_burst_packets;
        first_burst_packets.reserve(first_burst_num_packets);

        for(uint64_t i = 0, elements_finished = 0; i < first_burst_num_packets; i++, elements_finished += genconf.packet_numel) {
            struct DummyBackend::DummyPacket pkt;
            pkt.job_id = job_slice.job->id_;
            pkt.tensor.data_type = job_slice.slice.data_type;
            pkt.tensor.numel = std::min(genconf.packet_numel, job_slice.slice.numel - elements_finished);
            pkt.tensor.in_ptr = job_slice.slice.in_ptr;
            pkt.tensor.out_ptr = job_slice.slice.out_ptr;
            pkt.tensor.OffsetPtrs(elements_finished);
            pkt.packet_index = i;
            first_burst_packets.push_back(pkt);
            DLOG_IF(FATAL, pkt.packet_index != ((uintptr_t) pkt.tensor.in_ptr - (uintptr_t) job_slice.slice.in_ptr) / (genconf.packet_numel * DataTypeSize(job_slice.slice.data_type)))
                << "Something is wrong at first burst packet creation";
        }
        // TODO: Preprocess first burst of packets

        // Send first burst
        DVLOG(3) << "Worker thread '" << this->tid_ << "' will send the first '" << first_burst_packets.size() << "' packets";
        backend.SendBurst(this->tid_, first_burst_packets);
        ctx.GetStats().AddTotalPktsSent(this->tid_, first_burst_packets.size());

        // loop until all packets have been sent and received.
        DVLOG(3) << "Worker thread '" << this->tid_ << "' is starting the receive and send loop";
        uint64_t num_packets_received = 0; 
        while(num_packets_received != total_num_packets && ctx.GetContextState() == Context::ContextState::RUNNING) {
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
                << "' packets. Total received '" << num_packets_received << "/" << total_num_packets << "'.";
            
            // Create next group of packets to send including retransmissions.
            std::vector<DummyBackend::DummyPacket> packets_to_send;

            // TODO: Check timers and add packets requiring retransmission

            // Add new packets corresponding to received packets
            for(uint64_t i = 0; i < received_packets.size(); i++) {
                struct DummyBackend::DummyPacket pkt = received_packets.at(i);
                DVLOG(3) << "Worker thread '" << this->tid_ << "' retrieved packet '" << pkt.packet_index << "' with numel '" << pkt.tensor.numel << "'.";
                // If we had outstanding_pkts = 5 and we received packet with index 3 then the next packet index that will reuse the same slot is 3+5=8.
                pkt.tensor.OffsetPtrs(outstanding_pkts * genconf.packet_numel);
                if( (uintptr_t) pkt.tensor.in_ptr >= (uintptr_t) job_slice.slice.in_ptr + job_slice.slice.numel * DataTypeSize(job_slice.slice.data_type)) {
                    continue;
                }
                pkt.packet_index = ( (uintptr_t) pkt.tensor.in_ptr - (uintptr_t) job_slice.slice.in_ptr) / genconf.packet_numel;
                // The number of elements is either the maximum number of elements per packet,
                // or if this is the last packet, then its the remaining number of elements..
                pkt.tensor.numel = std::min(genconf.packet_numel, job_slice.slice.numel - genconf.packet_numel * pkt.packet_index);

                DVLOG(3) << "Worker thread '" << this->tid_ << "' creating packet '" << pkt.packet_index << "' with numel '" << pkt.tensor.numel << "'.";
                packets_to_send.push_back(pkt);
            }
            if(packets_to_send.size() == 0) {
                continue;
            }
            // TODO: Postprocess and preprocess group of packets.

            // Send the next group of packets
            DVLOG(3) << "Worker thread '" << this->tid_ << "' sending next burst containing '" << packets_to_send.size() << "' packets.";
            backend.SendBurst(this->tid_, packets_to_send);
            ctx.GetStats().AddTotalPktsSent(this->tid_, packets_to_send.size());

        } // while (num_packets_received != total_num_packets && ctx.GetContextState() == Context::ContextState::RUNNING)

        // TODO: PostprocessJobSlice
        
        // Notify the ctx that the worker thread finished this job slice.
        if(ctx.GetContextState() == Context::ContextState::RUNNING) {
            DVLOG(2) << "Worker thread '" << this->tid_ << "' notifying job slice completion with job id: " << job_slice.job->id_  << ".";
            ctx.NotifyJobSliceCompletion(this->tid_, job_slice);
        }

    } // while(ctx.GetContextState() == Context::ContextState::RUNNING)

    VLOG(0) << "Worker thread '" << this->tid_ << "' exiting.";
    backend.CleanupWorkerThread(this->tid_);
}

} // namespace switchml