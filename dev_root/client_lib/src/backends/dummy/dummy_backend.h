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
 * @file dummy_backend.h
 * @brief Declares the DummyBackend class.
 */

#ifndef SWITCHML_DUMMY_BACKEND_H_
#define SWITCHML_DUMMY_BACKEND_H_

#ifndef DUMMY
#define DUMMY // Just so that IDEs activate the dummy sections of the code
#endif

#include <thread>
#include <vector>

#include "common.h"
#include "backend.h"

#define DUMMY_ELEMENT_SIZE 4

namespace switchml {

// Forward declare DummyWorkerThread so we can add it as a member variable
class DummyWorkerThread;

/**
 * @brief A backend for debugging which simulates communication by sleeping.
 * 
 * It allows us to test the correctness of all components without having to deal with the complexities
 * of a real backend and without performing any actual communication.
 * The backend launches worker threads and sleeps when a send or receive is called.
 * The sleeping duration is determined by the dummy bandwidth and the size of the tensor.
 * The bandwidth is configurable through the configuration file.
 */
class DummyBackend : public switchml::Backend {
  public:

    /**
     * @brief A struct that describes the unit of transmission in the dummy backend (The DummyPacket).
     * 
     * A **JobSlice** is divided by the worker thread to multiple **DummyPacket* structs which then get sent then received using the backend.
     */
    struct DummyPacket{
      /** 
       * A packet identifier unique only within a job slice. 
       * Accessed only by the worker thread that created the message.
       * Can be calculated as packet offset from the job slice divided by the packet size 
       */
      uint64_t pkt_id;
      /** The identifier of the job from which this message came from */
      JobId job_id;
      /** The number of elements in the packet */
      Numel numel;
      /** The data type of the elements */
      DataType data_type;
      /** Pointer to data that is supposed to be outstanding (in the network) */
      void* entries_ptr;
      /** Pointer to extra info that is supposed to be outstanding (in the network) */
      void* extra_info_ptr;
    };

    /**
     * @brief Initialize members and allocate worker_threads and pending_messages arrays.
     * 
     * @param [in] context a reference to the switchml context.
     * @param [in] config a reference to the switchml configuration.
     */
    DummyBackend(Context& context, Config& config);

   /**
    * @brief Free worker_threads and pending_messages arrays.
    */
    ~DummyBackend();

    DummyBackend(DummyBackend const&) = delete;
    void operator=(DummyBackend const&) = delete;

    DummyBackend(DummyBackend&&) = default;
    DummyBackend& operator=(DummyBackend&&) = default;

    /**
     * @brief Creates and starts worker threads
     * @see CleanupWorker()
     */
    void SetupWorker() override;

    /**
     * @brief Stops worker threads
     * @see SetupWorker()
     */
    void CleanupWorker() override;

    /**
     * @brief Does nothing
     */
    void SetupWorkerThread(WorkerTid worker_thread_id);

    /**
     * @brief Does nothing
     */
    void CleanupWorkerThread(WorkerTid worker_thread_id);

    /**
     * @brief Sends a burst of packets specific to a worker thread.
     * 
     * This is a generic function that could be used to send a single message or multiple packets at once.
     * The function sleeps for a period equal to all packets sizes divided by the dummy backend bandwidth to
     * simulate network sending.
     * 
     * The sent packets are stored internally so that they can later be retrieved by ReceiveBurst()
     * 
     * @param [in] worker_thread_id The id of the calling worker thread.
     * @param [in] packets_to_send A vector of dummy packets to send.
     * @see ReceiveBurst()
     */
    void SendBurst(WorkerTid worker_thread_id, const std::vector<DummyPacket>& packets_to_send);

    /**
     * @brief Receives a burst of packets specific to a worker thread.
     * 
     * This function returns a random number of packets from the packets that the worker has sent using SendBurst()
     * The packets can be received out of order to simulate a real network.
     * Before the packets are returned, the elements are multiplied by the number of workers to simulate that 
     * an AllReduce Sum opearation took place.
     * 
     * @param [in] worker_thread_id The id of the calling worker thread.
     * @param [out] packets_received The vector to fill with packets received.
     * @see SendBurst()
     */
    void ReceiveBurst(WorkerTid worker_thread_id, std::vector<DummyPacket>& packets_received);
  
  private:
    void ProcessPacket(DummyPacket& msg);

    /** Stores all of the dummy worker threads */
    std::vector<DummyWorkerThread> worker_threads_;

    /** 
     * An array of vectors indexed by worker thread ids.
     * Each vector is used to store pending packets that have been sent by a worker thread using SendBurst()
     * so that it can be retrieved when the worker thread calls the ReceiveBurst() function.
     */
    std::vector<DummyPacket>* pending_packets_;
};

} // namespace switchml
#endif // SWITCHML_DUMMY_BACKEND_H_