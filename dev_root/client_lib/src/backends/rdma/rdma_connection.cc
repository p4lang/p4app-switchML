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
 * @file rdma_connection.cc
 * @brief Implement the RdmaConnection class.
 */

#include "rdma_connection.h"

#include <glog/logging.h>
#include <chrono>

#include "common_cc.h"
#include "config.h"
#include "rdma_backend.h"
#include "rdma_utils.h"

namespace switchml {

void RdmaConnection::PostRecv(ibv_qp* qp, ibv_recv_wr* wr) {
    ibv_recv_wr* bad_wr = nullptr;

    PCHECK(ibv_post_recv(qp, wr, &bad_wr) >= 0)
        << "Error posting receive WR " << wr << " id 0x" << std::hex
        << wr->wr_id << " QP 0x" << qp->qp_num << std::dec;

    CHECK(!bad_wr) << "Error posting receive WR at WR " << bad_wr << " id 0x"
                << std::hex << bad_wr->wr_id << " QP 0x" << qp->qp_num
                << std::dec;
}

void RdmaConnection::PostSend(ibv_qp* qp, ibv_send_wr* wr) {
    ibv_send_wr* bad_wr = nullptr;

    PCHECK(ibv_post_send(qp, wr, &bad_wr) >= 0)
        << "Error posting send WR " << wr << " id 0x" << std::hex << wr->wr_id
        << " QP 0x" << qp->qp_num << std::dec;

    CHECK(!bad_wr) << "Error posting send WR at WR " << bad_wr << " id 0x"
                << std::hex << bad_wr->wr_id << " QP 0x" << qp->qp_num
                << std::dec;
}

RdmaConnection::RdmaConnection(Config& config)
    : config_(config),
    endpoint_(config.backend_.rdma.device_name, config.backend_.rdma.device_port_id,
                config.backend_.rdma.gid_index),
    grpc_client_(config),
    memory_region_(nullptr),
    mtu_(config.general_.packet_numel == 256 ? IBV_MTU_1024 : IBV_MTU_256), // 256 elements = 1024 MTU
    // We create a queue pair for each outstanding message
    num_queue_pairs_(config.general_.max_outstanding_packets / 
                        (config.backend_.rdma.msg_numel / config.general_.packet_numel)),
    // A completion queue for each worker thread
    completion_queues_(config.general_.num_worker_threads, nullptr),
    queue_pairs_(num_queue_pairs_, nullptr),
    neighbor_gids_(num_queue_pairs_),
    neighbor_qpns_(num_queue_pairs_, 0),
    neighbor_psns_(num_queue_pairs_, 0),
    neighbor_rkeys_(num_queue_pairs_, 0)
{
    // Allocate buffer at same address on each node
    // The size of the buffer must be big enough to accomodate all the data that is outstanding.
    this->memory_region_ = this->endpoint_.AllocateAtAddress(
        (void*)(1L << 44), config.general_.packet_numel * config.general_.max_outstanding_packets * RDMA_SWITCH_ELEMENT_SIZE);

    DVLOG(1) << "Allocated " << memory_region_->length
                << "B buffer at address " << memory_region_->addr;
}

RdmaConnection::~RdmaConnection() { this->endpoint_.free(memory_region_); }

ibv_cq* RdmaConnection::GetWorkerThreadCompletionQueue(WorkerTid worker_thread_id) {
    return this->completion_queues_[worker_thread_id];
}

std::vector<ibv_qp*> RdmaConnection::GetWorkerThreadQueuePairs(WorkerTid worker_thread_id) {
    uint64_t qps_per_worker_thread = this->num_queue_pairs_ / this->config_.general_.num_worker_threads;
    std::vector<ibv_qp*>::iterator start =
        this->queue_pairs_.begin() + qps_per_worker_thread * worker_thread_id;
    std::vector<ibv_qp*>::iterator end = start + qps_per_worker_thread;

    return std::vector<ibv_qp*>(start, end);
}

std::vector<uint32_t> RdmaConnection::GetWorkerThreadRkeys(WorkerTid worker_thread_id) {
    uint64_t qps_per_worker_thread = this->num_queue_pairs_ / this->config_.general_.num_worker_threads;
    std::vector<uint32_t>::iterator start =
        this->neighbor_rkeys_.begin() + qps_per_worker_thread * worker_thread_id;
    std::vector<uint32_t>::iterator end = start + qps_per_worker_thread;
    return std::vector<uint32_t>(start, end);
}

std::pair<void*, uint32_t> RdmaConnection::GetWorkerThreadMemoryRegion(WorkerTid worker_thread_id) {
    uint64_t bytes_per_worker_thread = this->memory_region_->length / this->config_.general_.num_worker_threads;
    size_t offset = worker_thread_id * bytes_per_worker_thread;
    int8_t* advanced_ptr = static_cast<int8_t*>(this->memory_region_->addr) + offset;
    return std::make_pair(static_cast<void*>(advanced_ptr),
                          this->memory_region_->lkey);
}

RdmaEndpoint& RdmaConnection::GetEndpoint() {
    return this->endpoint_;
}


void RdmaConnection::Connect() {
    InitializeQueuePairs();

    // In order to move the queues to RTR, we now need to exchange GID,
    // queue pair numbers, and initial packet sequence numbers with
    // our neighbors
    ExchangeConnectionInfo();

    ConnectQueuePairs();
}

void RdmaConnection::InitializeQueuePairs() {
    // See section 3.5 of the RDMA Aware Networks Programming User
    // Manual for more details on queue pair bringup

    // Create shared completion queues, one per thread

    for (unsigned int i = 0; i < this->config_.general_.num_worker_threads; ++i) {
      this->completion_queues_[i] = this->endpoint_.CreateCompletionQueue();
      CHECK(this->completion_queues_[i]) << "Error creating completion queue";

      DVLOG(1) << "Created completion queue " << i;
    }

    // Create queue pairs. Spread queue pairs across threads with thread 0
    // having QPs 0 to n-1, thread 1 having QPs n to 2n-1, thread 2 having QPs
    // 2n to 3n-1, etc.
    unsigned int qps_per_worker_thread = this->num_queue_pairs_ / this->config_.general_.num_worker_threads;

    for (unsigned int i = 0; i < this->num_queue_pairs_; ++i) {
      // use the completion queue associated with the thread for this queue pair
      ibv_cq* completion_queue_for_qp = this->completion_queues_[i / qps_per_worker_thread];
      this->queue_pairs_[i] = this->endpoint_.CreateQueuePair(completion_queue_for_qp);
      DVLOG(1) << "Created queue pair " << i << ":0x" << std::hex
                 << this->queue_pairs_[i]->qp_num << std::dec;
    }

    // Move queue pairs to INIT. This generates a local queue pair number
    for (unsigned int i = 0; i < this->num_queue_pairs_; ++i) { MoveToInit(i); }

    // At this point we can post receive buffers. In theory, we
    // *should* do so before we move queues to RTR, but as long as we
    // have some other syncronization mechanism that will keep other
    // parties from sending before we're ready, it's okay not to.
}

// Exchange queue pair information
void RdmaConnection::ExchangeConnectionInfo() {
    DVLOG(1) << "Worker " << this->config_.general_.rank << "/" 
               << this->config_.general_.num_workers
               << " requesting connection to switch";

    // We set the session id as the current timestamp
    uint64_t session_id = 0;
    if (this->config_.general_.rank == 0) {
      auto current_time = std::chrono::high_resolution_clock::now();
      auto time_since_epoch = current_time.time_since_epoch();
      auto nanoseconds_since_epoch =
          std::chrono::duration_cast<std::chrono::nanoseconds>(
              time_since_epoch);
      session_id = nanoseconds_since_epoch.count();
      DVLOG(1) << "Session id is 0x" << std::hex << session_id << std::dec;
    }

    // Broadcast session id to other workers
    switchml_proto::BroadcastRequest bcast_request;
    switchml_proto::BroadcastResponse bcast_response;
    bcast_request.set_value(session_id);
    bcast_request.set_rank(this->config_.general_.rank);
    bcast_request.set_num_workers(this->config_.general_.num_workers);
    bcast_request.set_root(0);

    this->grpc_client_.Broadcast(bcast_request, &bcast_response);

    session_id = bcast_response.value();

    // Send connection request to coordinator
    switchml_proto::RdmaSessionRequest request;
    switchml_proto::RdmaSessionResponse response;
    request.set_session_id(session_id);
    request.set_rank(this->config_.general_.rank);
    request.set_num_workers(this->config_.general_.num_workers);
    request.set_mac(this->endpoint_.GetMac());
    request.set_ipv4(this->endpoint_.GetIPv4());
    request.set_rkey(this->memory_region_->rkey);
    request.set_packet_size(switchml_proto::PacketSize(mtu_));
    request.set_message_size(this->config_.backend_.rdma.msg_numel * RDMA_SWITCH_ELEMENT_SIZE); 

    for (uint32_t i = 0; i < this->num_queue_pairs_; ++i) {
      request.add_qpns(queue_pairs_[i]->qp_num);
      request.add_psns(queue_pairs_[i]->qp_num / 2);  // for debugging
    }

    switchml_proto::BarrierRequest barrier_request;
    switchml_proto::BarrierResponse barrier_response;
    barrier_request.set_num_workers(this->config_.general_.num_workers);

    if (this->config_.general_.rank == 0) {
      // First worker clears switch state before processing the request
      this->grpc_client_.CreateRdmaSession(request, &response);
      this->grpc_client_.Barrier(barrier_request, &barrier_response);

    } else {
      // Remaining workers process switch state after the first one is done
      this->grpc_client_.Barrier(barrier_request, &barrier_response);
      this->grpc_client_.CreateRdmaSession(request, &response);
    }

    // Ensure switch has gotten all workers' job state before proceeding
    this->grpc_client_.Barrier(barrier_request, &barrier_response);

    // Copy remote data to our neigbor arrays to continue queue pair setup
    for (unsigned int i = 0; i < this->num_queue_pairs_; ++i) {
      if (this->config_.backend_.rdma.gid_index >= 2) {  // IPv4-based GID
        this->neighbor_gids_[i] = IPv4ToGID(response.ipv4());
      } else {
        this->neighbor_gids_[i] = MACToGID(response.mac());
      }
      this->neighbor_rkeys_[i] = response.rkey();
      this->neighbor_qpns_[i] = response.qpns(i);
      this->neighbor_psns_[i] = response.psns(i);
    }
}

void RdmaConnection::ConnectQueuePairs() {
    // See section 3.5 of the RDMA Aware Networks Programming User
    // Manual for more details on queue pair bringup.

    // Move queue pairs to RTR. After this, we're ready to receive.
    for (uint32_t i = 0; i < this->num_queue_pairs_; ++i) { MoveToRtr(i); }

    // Move queue pairs to RTS. After this, we're ready to send.
    for (uint32_t i = 0; i < this->num_queue_pairs_; ++i) { MoveToRts(i); }
}

void RdmaConnection::MoveToInit(int qp_index) {
    ibv_qp_attr attributes;
    std::memset(&attributes, 0, sizeof(attributes));

    attributes.qp_state = IBV_QPS_INIT;
    attributes.port_num = this->config_.backend_.rdma.device_port_id;
    attributes.pkey_index = 0;
    attributes.qp_access_flags =
        (IBV_ACCESS_LOCAL_WRITE | IBV_ACCESS_REMOTE_WRITE);

    PCHECK(ibv_modify_qp(this->queue_pairs_[qp_index], &attributes,
                         IBV_QP_STATE | IBV_QP_PORT | IBV_QP_PKEY_INDEX |
                             IBV_QP_ACCESS_FLAGS) >= 0)
        << "Error setting queue pair to INIT";

    DVLOG(1) << "Queue pair " << qp_index << ": INIT";
}

void RdmaConnection::MoveToRtr(int qp_index) {
    ibv_qp_attr attributes;
    std::memset(&attributes, 0, sizeof(attributes));

    attributes.qp_state = IBV_QPS_RTR;
    attributes.dest_qp_num = this->neighbor_qpns_[qp_index];
    attributes.rq_psn = this->neighbor_psns_[qp_index];
    attributes.max_dest_rd_atomic = kMaxDestRdAtomic;  // used only for RC
    attributes.min_rnr_timer = kMinRnrTimer;           // used only for RC
    attributes.path_mtu = this->mtu_;
    attributes.ah_attr.is_global = 1;
    attributes.ah_attr.dlid = this->endpoint_.GetPortAttributes()
                                  .lid;  // not really necessary since using
                                         // RoCE, not IB, and is_global is set

    attributes.ah_attr.sl = 0;
    attributes.ah_attr.src_path_bits = 0;
    attributes.ah_attr.port_num = this->config_.backend_.rdma.device_port_id;
    attributes.ah_attr.grh.dgid = this->neighbor_gids_[qp_index];
    attributes.ah_attr.grh.sgid_index = this->config_.backend_.rdma.gid_index;
    attributes.ah_attr.grh.flow_label = 0;
    attributes.ah_attr.grh.hop_limit = 0xFF;
    attributes.ah_attr.grh.traffic_class = 1;

    PCHECK(ibv_modify_qp(this->queue_pairs_[qp_index], &attributes,
                         IBV_QP_STATE | IBV_QP_PATH_MTU | IBV_QP_DEST_QPN |
                             IBV_QP_RQ_PSN | IBV_QP_AV |
                             //(qp_type == IBV_QPT_RC
                             //     ? (IBV_QP_MAX_DEST_RD_ATOMIC |
                             //     IBV_QP_MIN_RNR_TIMER) : 0));
                             0) >= 0)
        << "Error setting queue pair to RTR";

    DVLOG(1) << "Connected QP 0x" << std::hex << this->queue_pairs_[qp_index]->qp_num
              << " with remote QP 0x" << this->neighbor_qpns_[qp_index] << std::dec
              << " initial PSN " << this->neighbor_psns_[qp_index];
    DVLOG(1) << "Queue pair " << qp_index << ": RTR";
}

void RdmaConnection::MoveToRts(int qp_index) {
    ibv_qp_attr attributes;
    std::memset(&attributes, 0, sizeof(attributes));

    attributes.qp_state = IBV_QPS_RTS;
    attributes.sq_psn =
        this->queue_pairs_[qp_index]->qp_num / 2;  // use QPN/2 as initial PSN (for testing)
    attributes.timeout = kTimeout;    // used only for RC
    attributes.retry_cnt = kRetryCount;       // used only for RC
    attributes.rnr_retry = kRnrRetry;         // used only for RC
    attributes.max_rd_atomic = kMaxRdAtomic;  // used only for RC

    PCHECK(ibv_modify_qp(this->queue_pairs_[qp_index], &attributes,
                         IBV_QP_STATE | IBV_QP_SQ_PSN) >= 0)
        << "Error setting queue pair to RTS";
    DVLOG(1) << "Queue pair " << qp_index << ": RTS";
}

}  // namespace switchml
