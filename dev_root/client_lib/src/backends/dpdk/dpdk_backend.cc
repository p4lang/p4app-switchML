/**
 * SwitchML Project
 * @file dpdk_backend.cc
 * @brief Implements the DpdkBackend class.
 */

#include "dpdk_backend.h"

#include <netinet/in.h>
#include <arpa/inet.h>

#include "common_cc.h"
#include "dpdk_worker_thread.h"
#include "dpdk_master_thread.h"
#include "dpdk_utils.h"

namespace switchml {

DpdkBackend::DpdkBackend(Context& context, Config& config)
    : Backend(context, config),
    grpc_client_(config)
{
    // Do nothing
}

DpdkBackend::~DpdkBackend() {
    // The destructor must be implemented here since the class members DpdkWorkerThread and DpdkMasterThread
    // are not fully defined in the header.

    // Do nothing
}

void DpdkBackend::SetupWorker() {

    VLOG(0) << "Setting up worker.";

    // Parse worker addresses from config

    // The mac address cannot be retrieved until rte_eth_dev_configure() is called in the master thread.

    struct in_addr w_ip;
    LOG_IF(FATAL, inet_aton(this->config_.backend_.dpdk.worker_ip_str.c_str(), &w_ip) == 0)
        << "Failed to parse ip address '" << this->config_.backend_.dpdk.worker_ip_str << "'.";
    this->worker_e2e_addr_be_.ip = w_ip.s_addr;

    // The actual worker port will be updated by each worker thread later
    this->worker_e2e_addr_be_.port = this->config_.backend_.dpdk.worker_port;

    // Create and start the master thread
    this->master_thread_ = std::make_shared<DpdkMasterThread>(this->context_, *this, this->config_);
    this->master_thread_->Start();
}

void DpdkBackend::SetupSwitch() {
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
    switchml_proto::UdpSessionRequest request;
    switchml_proto::UdpSessionResponse response;
    request.set_session_id(session_id);
    request.set_rank(this->config_.general_.rank);
    request.set_num_workers(this->config_.general_.num_workers);
    request.set_mac(ChangeMacEndianness(this->worker_e2e_addr_be_.mac)); // Set MAC in little endian
    request.set_ipv4(rte_be_to_cpu_32(this->worker_e2e_addr_be_.ip)); // Set IP in little endian

    // Categorize the length of the packet
    uint8_t pkt_len_enum;
    if (this->config_.general_.packet_numel < 64) {
        pkt_len_enum = 0;
    } else if (this->config_.general_.packet_numel < 128) {
        pkt_len_enum = 1;
    } else if (this->config_.general_.packet_numel < 256) {
        pkt_len_enum = 2;
    } else {
        pkt_len_enum = 3;
    }
    request.set_packet_size(switchml_proto::PacketSize(pkt_len_enum));

    switchml_proto::BarrierRequest barrier_request;
    switchml_proto::BarrierResponse barrier_response;
    barrier_request.set_num_workers(this->config_.general_.num_workers);

    if (this->config_.general_.rank == 0) {
      // First worker clears switch state before processing the request
      this->grpc_client_.CreateUdpSession(request, &response);
      this->grpc_client_.Barrier(barrier_request, &barrier_response);

    } else {
      // Remaining workers process switch state after the first one is done
      this->grpc_client_.Barrier(barrier_request, &barrier_response);
      this->grpc_client_.CreateUdpSession(request, &response);
    }

    this->switch_e2e_addr_be_.mac = ChangeMacEndianness(response.mac());
    this->switch_e2e_addr_be_.ip = rte_cpu_to_be_32(response.ipv4());
    // TODO: Get the port from the response
    this->switch_e2e_addr_be_.port = rte_cpu_to_be_16(48864);

    // Ensure switch has gotten all workers' job state before proceeding
    this->grpc_client_.Barrier(barrier_request, &barrier_response);
}

void DpdkBackend::CleanupWorker() {
    VLOG(0) << "Cleaning up worker.";
    this->master_thread_->Join();
}

struct DpdkBackend::E2eAddress& DpdkBackend::GetSwitchE2eAddr() {
    return this->switch_e2e_addr_be_;
}

struct DpdkBackend::E2eAddress& DpdkBackend::GetWorkerE2eAddr() {
    return this->worker_e2e_addr_be_;
}

std::vector<DpdkWorkerThread>& DpdkBackend::GetWorkerThreads() {
    return this->worker_threads_;
}

} // namespace switchml