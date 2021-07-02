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
 * @file dpdk_master_thread.h
 * @brief Declares the DpdkMasterThread class.
 */

#ifndef SWITCHML_DPDK_MASTER_THREAD_H_
#define SWITCHML_DPDK_MASTER_THREAD_H_

#include <thread>

#include "dpdk_backend.h"
#include "common.h"
#include "config.h"
#include "context.h"

namespace switchml {

/**
 * @brief A class that represents a single dpdk master thread.
 * 
 * A single instance is created of this thread.
 * The thread is responsible for creating, starting, and managing all of the dpdk worker threads.
 * 
 * @see DpdkWorkerThread
 */
class DpdkMasterThread{
  public:
    /**
     * @brief Initialize all members.
     * 
     * @param [in] context a reference to the switchml context.
     * @param [in] backend a reference to the created dpdk backend.
     * @param [in] config a reference to the context configuration.
     */
    DpdkMasterThread(Context& context, DpdkBackend& backend, Config& config);

    /**
     * @brief Deletes the reference to the system thread
     */
    ~DpdkMasterThread();

    DpdkMasterThread(DpdkMasterThread const&) = default;
    void operator=(DpdkMasterThread const&) = delete;

    DpdkMasterThread(DpdkMasterThread&&) = default;
    DpdkMasterThread& operator=(DpdkMasterThread&&) = default;

    /**
     * @brief This is the point of entry function for the thread.
     * 
     * The function starts by initializing EAL then starting worker threads.
     * Then the master thread itself becomes a worker thread.
     * Finally, when the master thread finishes its worker thread function it
     * waits for other threads and cleans up before exiting.
     */
    void operator()();

    /**
     * @brief Start the thread
     */
    void Start();

    /**
     * @brief Wait for the thread to exit and delete its system reference
     */
    void Join();

  private:
    /** A reference to the context */
    Context& context_;
    /** A reference to the context backend */
    DpdkBackend& backend_;
    /** A reference to the context configuration */
    Config& config_;
    /** A pointer to the actual system thread object */
    std::thread* thread_;
};

} // namespace switchml
#endif // SWITCHML_DPDK_MASTER_THREAD_H_