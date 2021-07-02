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
 * @file fifo_scheduler.h
 * @brief Declares the FifoScheduler class.
 */

#ifndef SWITCHML_FIFO_SCHEDULER_H_
#define SWITCHML_FIFO_SCHEDULER_H_

#include <queue>
#include <unordered_map>

#include "common.h"
#include "job.h"
#include "scheduler.h"
#include "utils.h"

namespace switchml {

/**
 * @brief A subclass of Scheduler that uses a single FIFO queue to store and dispatch jobs. 
 * 
 * Jobs are divided into almost-equally-sized job slices where each worker thread works on a single job slice.
 * 
 * This FifoScheduler uses a static mapping between the job slices and the worker threads.
 * That means each worker thread will get a known slice of each job and will not compete for slices.
 * For example:
 * If we had 3 worker threads and a job J where J.numel=24 then worker thread 0 will ALWAYS get a slice that includes
 * elements 0-7, thread 1 will ALWAYS get a slice including elements 8-15, thread 3 will ALWAYS get a slice including 16-23
 * The static mapping is done to avoid collisions at the switch because each worker thread is assigned a unique slot in
 * the switch (at least with the current p4 program version). And we want to make sure that for example elements 0-7 in worker node 0 and 
 * worker node 1 are all heading to the same slot in the switch.
 */
class FifoScheduler : public switchml::Scheduler {
  public:
    /**
     * @brief Initialize all the members
     * 
     * @param [in] config the switchml configuration.
     */
    FifoScheduler(Config& config);

    ~FifoScheduler() = default;

    FifoScheduler(FifoScheduler const&) = delete;
    void operator=(FifoScheduler const&) = delete;

    FifoScheduler(FifoScheduler&&) = default;
    FifoScheduler& operator=(FifoScheduler&&) = default;

    bool EnqueueJob(std::shared_ptr<Job> job) override;

    /**
     * @brief Get a job request slice.
     * 
     * This is called through the context by worker threads to get a job slice.
     * How the Job is sliced and distributed depends on the scheduler implementation.
     * The function should block the calling thread until a job slice is retrieved.
     * 
     * This function implements a worker thread barrier that ensures
     * that no worker thread gets ahead of other worker threads and that all worker threads
     * are working on the same job. This is unecessary but it allows us to use a single simple
     * queue with constant GetJobSlice time.
     * 
     * @param [in] worker_thread_id  The id of the worker thread that wants a job slice.
     * @param [out] job_slice  A reference to a job slice variable. 
     * @return true if the scheduler returned a valid job slice.
     * @return false the caller was forced to wakeup and the scheduler did not return a valid job slice.
     */
    bool GetJobSlice(WorkerTid worker_thread_id, JobSlice& job_slice) override;

    /**
     * @brief Signal the scheduler that a job slice has been finished.
     * 
     * @param [in] worker_thread_id The id of the worker thread that finished the job slice.
     * @param [in] job_slice The job slice that finished.
     * @return true If the job corresponding to this job slice has finished all its job slices.
     * @return false If there is still some job slices to be completed either by other worker threads.
     */
    bool NotifyJobSliceCompletion(WorkerTid worker_thread_i, const JobSlice& job_slice) override;

    /**
     * @brief calls Scheduler::Stop(), wakes up all threads waiting, and clears all queues.
     * 
     * After calling the super function Scheduler::Stop(), the functions destroys the barrier waking up all
     * threads that are waiting on the barrier.
     * Then the function sets all unfinished jobs to failed thus waking up any threads waiting on a specific job.
     * Finally, it clears queue_, undispatched_job_slices_, and undispatched_job_slices_
     */
    void Stop() override;

  private:
    /**
     * This simple fifo queue is the main data structure for this scheduler.
     * Jobs are added to the back, job slices are taken from the front.
     */
    std::queue<std::shared_ptr<Job>> queue_;
    
    /**
     * This map will store the number of job slices that finished for each job.
     * Once the number of job slices reaches the number worker threads, it means
     * the job is finished. 
     */
    std::unordered_map<JobId, int> finished_job_slices_;
    
    /** This map will store the number of job slices that are yet to be dispatched. */
    std::unordered_map<JobId, int> undispatched_job_slices_;

    /**
     * A synchronization barrier used by GetJobSlice
     */
    Barrier barrier_;
};

} // namespace switchml
#endif // SWITCHML_FIFO_SCHEDULER_H_