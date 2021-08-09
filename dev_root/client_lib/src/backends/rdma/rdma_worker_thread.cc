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
 * @file rdma_worker_thread.cc
 * @brief Implements the RdmaWorkerThread class.
 */

#include "rdma_worker_thread.h"

#include <stdlib.h>

#include "common_cc.h"
#include "rdma_utils.h"
#include "prepostprocessor.h"

namespace switchml {

WorkerTid RdmaWorkerThread::next_tid_ = 0;

RdmaWorkerThread::RdmaWorkerThread(Context& context, RdmaBackend& backend, Config& config) :
    tid_(RdmaWorkerThread::next_tid_++),
    context_(context),
    backend_(backend),
    config_(config),
    thread_(nullptr),
    ppp_(),
    completion_queue_(backend_.GetConnection()->GetWorkerThreadCompletionQueue(this->tid_)),
    queue_pairs_(backend_.GetConnection()->GetWorkerThreadQueuePairs(this->tid_)),
    send_sges_(this->queue_pairs_.size()),
    send_wrs_(this->queue_pairs_.size()),
    recv_wrs_(this->queue_pairs_.size()),
    msg_ids_(this->queue_pairs_.size(), 0),
    registered_buffer_ptr_(backend_.GetConnection()->GetWorkerThreadMemoryRegion(this->tid_).first),
    write_posted_count_per_qp_(this->queue_pairs_.size())
#ifdef TIMEOUTS
    ,timeouts_queue_(this->queue_pairs_.size(),
                     std::chrono::milliseconds(static_cast<long int>(config.general_.timeout)),
                     config.general_.timeout_threshold, config.general_.timeout_threshold_increment)
#endif
{
    // Do nothing
}

RdmaWorkerThread::~RdmaWorkerThread(){
    // Do nothing
}

void RdmaWorkerThread::operator()() {
    // Shortcuts
    Context& ctx = this->context_;
    RdmaBackend& bk = this->backend_;
    const GeneralConfig& genconf = this->config_.general_;
    const RdmaBackendConfig& rdmaconf = this->config_.backend_.rdma;

    // The number of packets in a message
    const uint64_t num_pkts_per_msg = rdmaconf.msg_numel/genconf.packet_numel;

    // The maximum number of outstanding messages for this worker.
    const uint64_t max_outstanding_msgs = genconf.max_outstanding_packets/ num_pkts_per_msg / genconf.num_worker_threads;

    // Bind thread on one core
    BindToCore(bk.GetConnection()->GetEndpoint().GetDevice(), this->tid_); 

    std::vector<uint32_t> rkeys = bk.GetConnection()->GetWorkerThreadRkeys(this->tid_);
    std::vector<ibv_wc> completions(this->queue_pairs_.size());

    CHECK(this->queue_pairs_.size() == max_outstanding_msgs);
    CHECK(this->queue_pairs_.size() == rkeys.size());

    // Size of a message in bytes.
    uint32_t msg_size = rdmaconf.msg_numel * RDMA_SWITCH_ELEMENT_SIZE;

    this->ppp_ = PrePostProcessor::CreateInstance(this->config_, this->tid_, msg_size, max_outstanding_msgs);

    // Initialize work requests
    for (uint16_t qpn = 0; qpn < this->queue_pairs_.size(); qpn++) {
        uint32_t wr_id = (this->tid_ << 16) | qpn;
        DVLOG(1) << "Worker " << this->tid_ << " initialized WRs " << wr_id;

        // Recv work requests
        std::memset(&this->recv_wrs_[qpn], 0, sizeof(this->recv_wrs_[qpn]));

        this->recv_wrs_[qpn].wr_id = wr_id;
        this->recv_wrs_[qpn].next = nullptr;
        this->recv_wrs_[qpn].sg_list = nullptr;
        this->recv_wrs_[qpn].num_sge = 0;

        // Initialize send SGE, the address is filled in later
        this->send_sges_[qpn].addr = 0;
        this->send_sges_[qpn].length = msg_size;
        this->send_sges_[qpn].lkey = bk.GetConnection()->GetWorkerThreadMemoryRegion(this->tid_).second;

        // Send work requests
        std::memset(&this->send_wrs_[qpn], 0, sizeof(this->send_wrs_[qpn]));
        this->send_wrs_[qpn].wr_id = wr_id;
        this->send_wrs_[qpn].next = nullptr;
        this->send_wrs_[qpn].sg_list = &this->send_sges_[qpn];
        this->send_wrs_[qpn].num_sge = 1;
        this->send_wrs_[qpn].opcode = IBV_WR_RDMA_WRITE_WITH_IMM;
        this->send_wrs_[qpn].send_flags = 0;
        this->send_wrs_[qpn].wr.rdma.remote_addr = 0;  // filled in later

        // Compute pool index. Lsb is pool flag. Set pool flag to
        // 1 initially; will be flipped to 0 before the first message is
        // posted
        uint16_t pool_index =
            (((this->queue_pairs_.size() * this->tid_ + qpn)  // base message-sized pool index
                * (num_pkts_per_msg)      // shifted to packet-sized pool index
                * 2)                           // leave space for pool bit
            | 1);                           // set pool bit initially

        // Write pool index into lower bits of rkey
        this->send_wrs_[qpn].wr.rdma.rkey = pool_index;
        VLOG(2) << "Worker " << this->tid_ << " QP " << qpn << ":0x" << std::hex
                << this->queue_pairs_[qpn]->qp_num << std::dec << " using rkey "
                << pool_index << " for remote rkey " << rkeys[qpn];
    }

    // Initialization complete

    // The job slice struct that will be filled with the next job slice to work on.
    JobSlice job_slice;

    // Main worker thread loop
    // This is where optimization is very important
    while(ctx.GetContextState() == Context::ContextState::RUNNING) {
        // Get a job slice
        bool got_job_slice = ctx.GetJobSlice(this->tid_, job_slice);
        if(!got_job_slice) {
            continue;
        }
        DVLOG(2) << "Worker thread '" << this->tid_ << "' received job slice with job id: " << job_slice.job->id_ << " with numel: " << job_slice.slice.numel << ".";
        if(job_slice.slice.numel <=0 || this->config_.general_.instant_job_completion) {
            if(ctx.GetContextState() == Context::ContextState::RUNNING) {
                DVLOG(2) << "Worker thread '" << this->tid_ << "' notifying job slice completion with job id: " << job_slice.job->id_  << ".";
                ctx.NotifyJobSliceCompletion(this->tid_, job_slice);
            }
            continue;
        }

        // Compute number of messages needed for this job
        uint64_t total_num_msgs = this->ppp_->SetupJobSlice(&job_slice);

        // We can logically divide all of the messages that we will send into 'max_outstanding_msgs' sized groups
        // (Or less in case the total number of messages was less than max_outsanding_msgs).
        // We call each of these groups a batch. So if max_outstanding_msgs=10 and we wanted to send 70 messages then we have 7 batches.
        uint64_t batch_num_msgs = std::min(max_outstanding_msgs, total_num_msgs);

        if(this->ppp_->NeedsExtraBatch()) {
            total_num_msgs += batch_num_msgs;
        }

        // Initialize msg_ids for the qps that we will use.
        for(size_t qpn = 0; qpn < batch_num_msgs; qpn++) {
            this->msg_ids_[qpn] = qpn;
        }

        // Initialize statistic variables.
        // We found that using local variables for the inner loops speeds things up.
        // We then push those local variables to the context statistics object at the end of each job.
        uint64_t stats_wrong_pkts_received = 0;
        uint64_t stats_correct_pkts_received = 0;
        uint64_t stats_total_pkts_sent = 0;
#ifdef TIMEOUTS
        uint64_t stats_timeouts = 0;
#endif

        DVLOG(3) << "Worker thread '" << this->tid_ << "' will send a total of '" << total_num_msgs << "' messages each having '" << rdmaconf.msg_numel << " elements.";

        // Send first batch
        DVLOG(3) << "Worker thread '" << this->tid_ << "' will send the first '" << batch_num_msgs << "' messages";
        for (uint16_t qpn = 0; qpn < batch_num_msgs; qpn++) {
            // Post recv work request
            this->PostRecvWr(qpn);
            // Post send work request
            this->PostSendWr(qpn);
        }
        stats_total_pkts_sent += num_pkts_per_msg*batch_num_msgs; 

        // loop until all messages have been sent and received.
        DVLOG(3) << "Worker thread '" << this->tid_ << "' is starting the receive and send loop";
        uint64_t num_received_msgs = 0;
        while (num_received_msgs < total_num_msgs && ctx.GetContextState() == Context::ContextState::RUNNING) {
            // Check for completions indicating received messages
            int cnum = ibv_poll_cq(this->completion_queue_, this->queue_pairs_.size(), &completions[0]);

            LOG_IF(FATAL, cnum < 0) << "Worker thread '" << this->tid_ << "' Failed polling completion queue with status " << cnum;

            // Process all the messages that we've received.
            uint64_t iteration_num_received_msgs = 0;
            for (int i = 0; i < cnum; ++i) {
                CHECK_EQ(completions[i].status, IBV_WC_SUCCESS)
                    << "Worker thread '" << this->tid_ << "' "
                    << " received completion error for work request id " << completions[i].wr_id
                    << " with status "
                    << ibv_wc_status_str(completions[i].status)
                    << " opcode " << completions[i].opcode;
                
                if (completions[i].opcode == IBV_WC_RECV_RDMA_WITH_IMM) {
                    // This completion indicates a received message
                    
                    CHECK_NE(completions[i].wc_flags & IBV_WC_WITH_IMM, 0) << "Worker thread '" << this->tid_ << "' Received message without immediate value";

                    // Select the first two bytes of the work request id which constitute the qpn
                    // relative to this worker thread.
                    uint16_t qpn = completions[i].wr_id & 0xFFFF;

                    uint16_t received_short_msg_id = completions[i].imm_data & 0xFFFF;
                    uint16_t expected_short_msg_id = msg_ids_[qpn] & 0xFFFF;

                    // TODO: Is this the message that we are expecting from this qpn?
                    // if(received_short_msg_id != expected_short_msg_id) {
                    //     if(received_short_msg_id < expected_short_msg_id && received_short_msg_id % batch_num_msgs == expected_short_msg_id % batch_num_msgs) {
                    //         DVLOG(3) << "Worker thread '" << this->tid_ << "' received duplicate message"
                    //             << " for qpn=" << qpn << ". Expected " << expected_short_msg_id << " But received " << received_short_msg_id;
                    //         PostRecvWr(qpn);
                    //     } else {
                    //         LOG(FATAL) << "Worker thread '" << this->tid_ << "' received unexpected message id"
                    //             << " for qpn=" << qpn << ". Expected " << expected_short_msg_id << " But received " << received_msg_id;
                    //     }
                    //     stats_wrong_pkts_received += num_pkts_per_msg;
                    //     continue;
                    // }

                    // This is a correct message, postprocess it.
                    void* message_start = static_cast<uint8_t*>(this->registered_buffer_ptr_) + qpn * msg_size;
                    uint8_t* imm_data = static_cast<uint8_t*>((void*)&completions[i].imm_data);
                    uint8_t* extra_info_ptr = imm_data + 2;
                    this->ppp_->PostprocessSingle(msg_ids_[qpn], message_start, extra_info_ptr);

                    // Increment message id
                    this->msg_ids_[qpn] += batch_num_msgs;
#ifdef TIMEOUTS
                    // The message for this qpn was received correctly, remove its timer.
                    this->timeouts_queue_.Remove(qpn);
#endif
                    iteration_num_received_msgs++;
                    // Post next send for this slot if needed
                    if (this->msg_ids_[qpn] < total_num_msgs) {
                        // Post recv work request
                        PostRecvWr(qpn);
                        // Post send work request
                        PostSendWr(qpn);
                        stats_total_pkts_sent += num_pkts_per_msg; 
                    }
                }  else if (completions[i].opcode == IBV_WC_RDMA_WRITE) {
                    // This completion indicates a successfully transmitted message
                    DVLOG(3) << "Worker thread '" << this->tid_ << "' "
                            << "received WRITE completion for "
                            << completions[i].wr_id << " for QP "
                            << completions[i].qp_num << " source "
                            << completions[i].src_qp;
                } else {
                    // This completion is unknown and should not have happenned.
                    LOG(FATAL) << "Worker thread '" << this->tid_ << "' "
                        << "received unknown successful completion with ID "
                        << completions[i].wr_id << " for QP 0x" << std::hex
                        << completions[i].qp_num << " source 0x"
                        << completions[i].src_qp << std::dec;
                }
            }

            num_received_msgs += iteration_num_received_msgs;
            stats_correct_pkts_received += num_pkts_per_msg * iteration_num_received_msgs;
            DVLOG_IF(3, iteration_num_received_msgs > 0) << "Worker thread '" << this->tid_ << "' received " 
                << iteration_num_received_msgs << " messages " << num_received_msgs << "/" << total_num_msgs << ".";

#ifdef TIMEOUTS
            // Check for timeouts
            int qpn = this->timeouts_queue_.Check(switchml::clock::now());
            // If qpn >= 0 that means a slot has timed out. Retransmit its message..
            if (qpn >= 0) {
                stats_timeouts += num_pkts_per_msg; 
                // Post send work request (no recv work request is posted and preprocess is set to false)
                PostSendWr((uint16_t)qpn, false);
                stats_total_pkts_sent += num_pkts_per_msg; 
            }
            
#endif
        } // while (num_received_msgs < total_num_msgs && ctx.GetContextState() == Context::ContextState::RUNNING)

        this->ppp_->CleanupJobSlice();

        // Add the local stats to the context stats object.
        ctx.GetStats().AddCorrectPktsReceived(this->tid_, stats_correct_pkts_received);
        ctx.GetStats().AddTotalPktsSent(this->tid_, stats_total_pkts_sent);
        ctx.GetStats().AddWrongPktsReceived(this->tid_, stats_wrong_pkts_received);
#ifdef TIMEOUTS
        ctx.GetStats().AddTimeouts(this->tid_, stats_timeouts);
#endif
        // Finally notify the ctx that the worker thread finished this job slice.
        if(ctx.GetContextState() == Context::ContextState::RUNNING) {
            DVLOG(2) << "Worker thread '" << this->tid_ << "' notifying job slice completion with job id: " << job_slice.job->id_  << ".";
            ctx.NotifyJobSliceCompletion(this->tid_, job_slice);
        }
    } // while(ctx.GetContextState() == Context::ContextState::RUNNING)
}

void RdmaWorkerThread::PostRecvWr(uint16_t qpn) {
    this->backend_.GetConnection()->PostRecv(this->queue_pairs_[qpn], &this->recv_wrs_[qpn]);
}

void RdmaWorkerThread::PostSendWr(uint16_t qpn, bool preprocess) {

    uint64_t msg_size = this->config_.backend_.rdma.msg_numel * RDMA_SWITCH_ELEMENT_SIZE;
    // Point at start of this message's data
    void* message_start = static_cast<uint8_t*>(this->registered_buffer_ptr_) + (qpn * msg_size);
    this->send_sges_[qpn].addr = (intptr_t) message_start;

    // Writes must be occasionally signaled because only upon a completion
    // event resources are deallocated from the send queue
    if ((this->write_posted_count_per_qp_[qpn] % 1024) == 0) {
        this->send_wrs_[qpn].send_flags = IBV_SEND_SIGNALED;
    } else {
        this->send_wrs_[qpn].send_flags = 0;
    }

    // Flip the pool flag
    this->send_wrs_[qpn].wr.rdma.rkey ^= 1;

    // Set destination address equal to the address where we want to
    // receive. We want to reuse the same buffer to send and recv
    this->send_wrs_[qpn].wr.rdma.remote_addr = send_sges_[qpn].addr;

    // Use the first 16 bits of the message id as a sequence number in the immediate field.
    // It will be sent back to check received messages for late, duplicated retrasmissions
    // In addition to the message id, the immediate field will also carry the extra
    // information needed by the prepostprocessor (Typically the exponenet).
    this->send_wrs_[qpn].imm_data = this->msg_ids_[qpn] & 0xFFFF;

    if (preprocess) {    
        // Compute extra info address
        uint8_t* imm_data = static_cast<uint8_t*>((void*) & this->send_wrs_[qpn].imm_data);
        uint8_t* extra_info_ptr = imm_data + 2; // Select the third least significant byte.
        
        // Preprocess the whole message. (This is where loading and quantizing the data is performed)
        // There is room in the immediate data to preprocess half the message at a time allowing for more
        // controlled quantization. But we don't need to do that unless we measure losses in accuracy upon
        // quantizing at the whole message scale.
        this->ppp_->PreprocessSingle(this->msg_ids_[qpn], message_start, extra_info_ptr);
    }

    DVLOG(3) << "Worker thread '" << this->tid_ << "' QP " << qpn << ":0x" << std::hex
            << this->queue_pairs_[qpn]->qp_num << std::dec << " posting write from "
            << (void*)this->send_sges_[qpn].addr << " length "
            << this->send_sges_[qpn].length << " rkey/slot " << std::hex
            << this->send_wrs_[qpn].wr.rdma.rkey << std::dec;

    this->backend_.GetConnection()->PostSend(this->queue_pairs_[qpn], &this->send_wrs_[qpn]);

#ifdef TIMEOUTS
    this->timeouts_queue_.Push(qpn, switchml::clock::now());
#endif

    write_posted_count_per_qp_[qpn]++;
}

void RdmaWorkerThread::Start() {
    LOG_IF(FATAL, this->thread_ != nullptr) << "Trying to start a thread twice.";
    // SUGGESTION: This works but is it the best way to to store a reference in the same class?
    this->thread_ = new std::thread(*this); // This will create a new thread that calls the operator()() function.
}

void RdmaWorkerThread::Join() {
    LOG_IF(FATAL, this->thread_ == nullptr) << "Trying to join a thread that hasn't started or has already been joined.";
    this->thread_->join();
    delete this->thread_;
}

} // namespace switchml