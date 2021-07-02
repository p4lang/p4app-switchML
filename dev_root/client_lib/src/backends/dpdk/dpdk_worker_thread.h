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
 * @file dpdk_worker_thread.h
 * @brief Declares the DpdkWorkerThread class.
 */

#ifndef SWITCHML_DPDK_WORKER_THREAD_H_
#define SWITCHML_DPDK_WORKER_THREAD_H_

#include <thread>
#include <memory>

#include "dpdk_backend.h"
#include "common.h"
#include "config.h"
#include "context.h"
#include "prepostprocessor.h"

#ifdef TIMEOUTS
#include <rte_timer.h>
#endif

namespace switchml {

/**
 * @brief A class that represents a single dpdk worker thread.
 * 
 * A worker thread constantly asks the context for work and carries it out.
 * 
 * Multiple instances of this class is typically created depending on
 * the number of cores in the configuration.
 * This class has no Start and Join functions as other typical thread classes in the client library.
 * This is because starting and joining the DPDK worker thread is handled by DPDK itself.
 */
class DpdkWorkerThread{
  public:
    /**
     * @brief Initialize all members and instantiate the prepostprocessor to be used by the worker thread.
     * 
     * @param [in] context a reference to the switchml context.
     * @param [in] backend a reference to the created dpdk backend.
     * @param [in] config a reference to the context configuration.
     */
    DpdkWorkerThread(Context& context, DpdkBackend& backend, Config& config);

    ~DpdkWorkerThread(); 

    DpdkWorkerThread(DpdkWorkerThread const&) = delete;
    void operator=(DpdkWorkerThread const&) = delete;

    DpdkWorkerThread(DpdkWorkerThread&&) = default;
    DpdkWorkerThread& operator=(DpdkWorkerThread&&) = default;

    /**
     * @brief This is the point of entry function for the thread.
     */
    void operator()();

    /** Worker thread id */
    const WorkerTid tid_;

  private:

    /** 
     * DPDK Callbacks cannot be bound to a class so we declare them in the switchml space
     * and allow them to access the private members of this worker thread.
     */
    friend void TxBufferCallback(struct rte_mbuf **pkts, uint16_t unsent, void *userdata);

    /** Monotonically increasing counter to give unique IDs for each new worker thread **/
    static WorkerTid next_tid_;
    /** A reference to the context */
    Context& context_;
    /** A reference to the context backend */
    DpdkBackend& backend_;
    /** A reference to the context configuration */
    Config& config_;

    /** What this worker thread will use as the source address for packets. (Big endian) */
    DpdkBackend::E2eAddress worker_thread_e2e_addr_be_;

    /** Worker thread core id */
    uint16_t lcore_id_;

    /** The prepostprocessor used by the worker thread */
    std::shared_ptr<PrePostProcessor> ppp_;

#ifdef TIMEOUTS
    friend void ResendPacketCallback(struct rte_timer *timer, void *arg);

    /** 
     * The number of clock cycles before timing out.
     * This field is changed by the ResendPacketCallback to adjust the
     * timeout based on the number of timeouts that occured.
     */
    uint64_t timer_cycles_;
#endif

};

} // namespace switchml
#endif // SWITCHML_DPDK_WORKER_THREAD_H_