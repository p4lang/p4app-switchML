/**
 * SwitchML Project
 * @file dpdk_master_thread.cc
 * @brief Implements the DpdkMasterThread class.
 */

#include "dpdk_master_thread.h"

#include <stdio.h>
#include <stdlib.h>
#include <bits/stdc++.h>
#include <rte_eal.h>
#include <rte_lcore.h>
#include <rte_errno.h>
#include <rte_ethdev.h>

#include <algorithm>

#include "common_cc.h"
#include "context.h"
#include "dpdk_worker_thread.h"

#include "dpdk_master_thread_utils.inc"

namespace switchml {

DpdkMasterThread::DpdkMasterThread(Context& context, DpdkBackend& backend, Config& config) :
context_(context),
backend_(backend),
config_(config),
thread_(nullptr)
{
    // Do nothing
}

DpdkMasterThread::~DpdkMasterThread(){
    if(this->thread_!=nullptr) {
        delete this->thread_;
    }
}

void DpdkMasterThread::operator()() {
    VLOG(0) << "Master thread starting.";

    int ret;
    // Prepare EAL arguments
    std::string eal_cmdline = "switchml -l " + this->config_.backend_.dpdk.cores_str + " "
        + this->config_.backend_.dpdk.extra_eal_options;
    VLOG(1) << "Initializing EAL with arguments '" << eal_cmdline << "'";
    std::vector <std::string> eal_cmdline_tokens;
    std::stringstream ss(eal_cmdline);
    std::string token;
    while(getline(ss, token, ' ')) {
        eal_cmdline_tokens.push_back(token);
    }
    int eal_args_c = eal_cmdline_tokens.size();
    char* eal_args[eal_args_c];
    for (int i = 0; i < eal_args_c; i++)
    {
        eal_args[i] = new char[eal_cmdline_tokens[i].size()];
        strcpy(eal_args[i], eal_cmdline_tokens[i].c_str());
    }
    
    // Initialize EAL
    ret = rte_eal_init(eal_args_c, eal_args);
    LOG_IF(FATAL, ret < 0) << "EAL init failed: " << rte_strerror(rte_errno);

    // Count the number of cores
    uint16_t num_cores = 1; // 1 for the master thread's core
    uint16_t lcore_id;
    RTE_LCORE_FOREACH_SLAVE(lcore_id) {
        num_cores++;
    }

    // Initialize the port
    uint16_t port_id = this->config_.backend_.dpdk.port_id;
    InitPort(this->config_.backend_.dpdk, num_cores);

    // Get the mac address 
    struct rte_ether_addr w_mac;
    ret = rte_eth_macaddr_get(port_id, &w_mac);
    LOG_IF(FATAL,  ret < 0) << "Failed to get mac address using port id '" << port_id << "'";
    memcpy(this->backend_.GetWorkerE2eAddr().mac, &w_mac, 6); 

    // Check state of slave cores
    RTE_LCORE_FOREACH_SLAVE(lcore_id)
    {
        LOG_IF(FATAL, rte_eal_get_lcore_state(lcore_id) != WAIT) << "Core " << lcore_id 
            << " in state " << rte_eal_get_lcore_state(lcore_id);
    }

    // Start a worker thread instance on each core
    std::vector<DpdkWorkerThread>& wts = this->backend_.GetWorkerThreads();
    wts.reserve(num_cores);
    int i = 0;
    RTE_LCORE_FOREACH_SLAVE(lcore_id) {
        wts.push_back(DpdkWorkerThread(this->context_, this->backend_, this->config_));
        ret = rte_eal_remote_launch(LaunchDpdkWorkerThread, &wts[i], lcore_id);
        LOG_IF(ERROR, ret != 0) << "Core " << lcore_id << " returned " << ret ;
        i++;
    }
    // Run the last worker thread on this master thread core
    wts.push_back(DpdkWorkerThread(this->context_, this->backend_, this->config_));
    wts[i](); 

    // Wait for worker threads
    VLOG(1) << "Master thread waiting for slave threads.";
    RTE_LCORE_FOREACH_SLAVE(lcore_id) {
        ret = rte_eal_wait_lcore(lcore_id);
        LOG_IF(ERROR, ret != 0) << "Core " << lcore_id << " returned " << ret ;
    }

    VLOG(1) << "Master thread closing port.";
    struct rte_flow_error error;
    LOG_IF(ERROR, rte_flow_flush(port_id, &error)!=0) << "Flow flush failed!";
    rte_eth_dev_stop(port_id);
    rte_eth_dev_close(port_id);
    
    VLOG(0) << "Master thread exiting.";
}

void DpdkMasterThread::Start() {
    LOG_IF(FATAL, this->thread_ != nullptr) << "Trying to start a thread twice.";
    // SUGGESTION: This works but is it the best way to to store a reference in the same class?
    this->thread_ = new std::thread(*this); // This will create a new thread that calls the operator()() function.
}

void DpdkMasterThread::Join() {
    LOG_IF(FATAL, this->thread_ == nullptr) << "Trying to join a thread that hasn't started or has already been joined.";
    this->thread_->join();
    delete this->thread_;
}

} // namespace switchml