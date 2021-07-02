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
 * @file job.h
 * @brief Declares the Job class alongside its needed structures and enums.
 */

#ifndef SWITCHML_JOB_H_
#define SWITCHML_JOB_H_

#include <mutex>
#include <condition_variable>
#include <atomic>

#include "common.h"

namespace switchml {

/**
 * @brief The type of collective communication job.
 */
enum JobType {
    ALLREDUCE, /**< Perform an AllReduce operation */
    BROADCAST /**< Perform a Broadcast operation. **Not yet supported** */
};

/**
 * @brief The operation to use when performing AllReduce.
 */
enum AllReduceOperation {
    SUM, /**< Use summation to reduce the tensors */
};

/**
 * @brief Extra information specific to the collective communication job.
 */
union ExtraJobInfo{
    AllReduceOperation allreduce_operation; /**< The operation to use for AllReduce */
    int32_t broadcast_root_rank; /**< The worker that is broadcasting so it knows that it should send and others will receive. */
};

/**
 * @brief Describes the current status of a Job instance.
 */
enum JobStatus {
    INIT,     /**< The job was just created. */
    QUEUED,   /**< The job has been added to the scheduler's queue. */
    RUNNING,  /**< Some worker threads are currently working on slices of the job. */
    FINISHED, /**< All job slices have been completed and the job finished successfully. */
    FAILED    /**< The job failed for some reason. */
};

/**
 * @brief A Job is used to represent work to be done by SwitchML.
 * 
 * It is created by the Context when an operation is requested,
 * submitted to the Scheduler, then the scheduler creates instances of JobSlice from it
 * to give it to the worker threads.
 */
class Job {
public:
    /**
     * @brief Construct a new Job object
     * 
     * @param [in] tensor The tensor to work on for this job.
     * @param [in] job_type The type of the job.
     * @param [in] extra_job_info Extra information that might be needed for the job.
     */
    Job(Tensor tensor, JobType job_type, ExtraJobInfo extra_job_info);

    ~Job() = default;
    
    Job(Job const&) = delete;
    void operator=(Job const&) = delete;

    Job(Job&&) = default;
    Job& operator=(Job&&) = default;

    /**
     * @brief Block the calling thread until the job completes or fails.
     */
    void WaitToComplete();

    /**
     * @brief Get the job's status.
     * 
     * @return JobStatus 
     */
    JobStatus GetJobStatus();

    /**
     * @brief Update the job's status and notify waiting threads if needed.
     * 
     * This function must only be called by the scheduler or the context.
     * JobStatus must progress in an increasing order.
     * 
     * @param [in] job_status the new job_status
     */
    void SetJobStatus(JobStatus job_status);

    /** Unique identifier for the job. */
    const JobId id_;
    /** Tensor to perform the collective communication job on. */
    const Tensor tensor_;
    /** The type of collective communication that the job will do. */
    const JobType job_type_;
    /** Extra information specific to the collective communication job. */
    const ExtraJobInfo extra_job_info_;

private:
    /** Monotonically increasing counter to give unique IDs for each new job **/
    static JobId next_id_;
    /** Describes the current status of the job. */
    std::atomic<JobStatus> job_status_;
    
    /** Mutex to be used with the job_finished_event_ */
    std::mutex access_mutex_;
    /** An event that signifies that the job has finished */
    std::condition_variable job_finished_event_;
};

/**
    * @brief A job slice that represents a part of a job.
    * 
    * This struct is what worker threads receive from the scheduler
    * and what they work on.
    */
struct JobSlice {
    /** A reference to the original job of which this slice came from. */
    std::shared_ptr<Job> job;
    /** The slice that the worker thread should work on */
    Tensor slice;
};

} // namespace switchml
#endif // SWITCHML_JOB_H_