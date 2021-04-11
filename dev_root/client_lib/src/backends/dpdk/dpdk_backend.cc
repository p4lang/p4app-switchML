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
    : Backend(context, config)
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

    // Parse switch addresses from config

    struct rte_ether_addr s_mac = Str2Mac(this->config_.backend_.dpdk.switch_mac_str, false);
    memcpy(this->switch_e2e_addr_be_.mac, s_mac.addr_bytes, 6);

    struct in_addr s_ip;
    LOG_IF(FATAL, inet_aton(this->config_.backend_.dpdk.switch_ip_str.c_str(), &s_ip) == 0)
        << "Failed to parse ip address '" << this->config_.backend_.dpdk.switch_ip_str << "'.";
    this->switch_e2e_addr_be_.ip = s_ip.s_addr;

    this->switch_e2e_addr_be_.port = rte_cpu_to_be_16(this->config_.backend_.dpdk.switch_port);

    // Parse worker addresses from config

    // The mac address cannot be retrieved until rte_eth_dev_configure() is called in the master thread.

    struct in_addr w_ip;
    LOG_IF(FATAL, inet_aton(this->config_.backend_.dpdk.worker_ip_str.c_str(), &w_ip) == 0)
        << "Failed to parse ip address '" << this->config_.backend_.dpdk.worker_ip_str << "'.";
    this->worker_e2e_addr_be_.ip = w_ip.s_addr;

    // The actual worker port will be updated by each worker thread later
    this->worker_e2e_addr_be_.port = 0;

    // Create and start the master thread
    this->master_thread_ = std::make_shared<DpdkMasterThread>(this->context_, *this, this->config_);
    this->master_thread_->Start();
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