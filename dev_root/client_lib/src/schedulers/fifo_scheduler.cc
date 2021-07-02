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
 * @brief Implements the FifoScheduler class.
 */

#include "common_cc.h"
#include "fifo_scheduler.h"
#include "config.h"
#include "context.h"

namespace switchml {

FifoScheduler::FifoScheduler(Config& config)
    : Scheduler(config)
    , queue_()
    , finished_job_slices_()
    , undispatched_job_slices_()
    , barrier_(config.general_.num_worker_threads)
{
    // nothing to do here
}

bool FifoScheduler::EnqueueJob(std::shared_ptr<Job> job) {
    std::unique_lock<std::mutex> lock(this->access_mutex_);
    job->SetJobStatus(JobStatus::QUEUED);
    this->finished_job_slices_.insert({job->id_, 0});
    this->undispatched_job_slices_.insert({job->id_, this->config_.general_.num_worker_threads});
    this->queue_.push(job);
    DVLOG(2) << "Queued job id: " << job->id_ << " job_type: "
        << job->job_type_ << " numel: " << job->tensor_.numel << " data_type: " << job->tensor_.data_type;
    this->job_submitted_event_.notify_all();
    return true;
}

bool FifoScheduler::GetJobSlice(WorkerTid worker_thread_id, JobSlice& job_slice) {
    DVLOG(2) << "Worker thread '" << worker_thread_id << "' is asking for a job slice.";
    std::unique_lock<std::mutex> lock(this->access_mutex_);

    if(this->stopped_) {
        return false;
    }

    const int num_worker_threads = this->config_.general_.num_worker_threads;

    // Wait for other workers
    lock.unlock();
    this->barrier_.Wait();
    lock.lock();

    // Block until we have a job. If the queue already has jobs then the thread will continue immediately.
    DVLOG_IF(2, !this->stopped_ && this->queue_.empty()) << "Worker thread '" << worker_thread_id << "' waiting for a job.";
    this->job_submitted_event_.wait(lock, [this] {
         return this->stopped_ || !this->queue_.empty();
    });

    // If we were forced to stop then return false.
    if(this->stopped_) {
        return false;
    }

    // ## Construct job slice ##
    std::shared_ptr<Job> job = this->queue_.front();

    int& job_slices_left = this->undispatched_job_slices_.at(job->id_);
    job_slices_left--;
    // If this is the last slice of the job then remove the job from the queue.
    if (job_slices_left == 0) {
        this->queue_.pop();
        this->undispatched_job_slices_.erase(job->id_);
    }

    job_slice.job = job;
    job_slice.slice = job->tensor_;

    // How many elements should this thread work on?
    job_slice.slice.numel = job->tensor_.numel / num_worker_threads;
    int remainder = job->tensor_.numel % num_worker_threads;
    // What about the remainder elements? Divide those across threads.
    // SUGGESTION: the number of extra elements is too small for us to care. Just give it to the first thread.
    bool get_extra_element = remainder > worker_thread_id;
    Numel offset;
    if (get_extra_element) {
        job_slice.slice.numel++;
        // If this worker thread got an extra element, then all previous
        // worker threads also got an extra element.
        offset = worker_thread_id * job_slice.slice.numel;
    } else {
        // If this worker thread did not get an extra element, then the remainder elements
        // have been added across previous threads.
        offset = worker_thread_id * job_slice.slice.numel + remainder;
    }
    job_slice.slice.OffsetPtrs(offset);
    // Set job status to running
    job->SetJobStatus(JobStatus::RUNNING);

    DVLOG(2) << "A job slice from job id: " << job_slice.job->id_ << " with offset: " << offset << " numel: " << job_slice.slice.numel
        << " was given to worker thread '" << worker_thread_id << "'.";
    return true;
}

bool FifoScheduler::NotifyJobSliceCompletion(WorkerTid worker_thread_id, const JobSlice& job_slice){
    std::unique_lock<std::mutex> lock(this->access_mutex_);
    if(this->stopped_) {
        return false;
    }
    int& finished_job_slices = this->finished_job_slices_.at(job_slice.job->id_);
    finished_job_slices++;
    DVLOG(2) << "Worker thread '" << worker_thread_id << "' has finished its job slice for job id: " << job_slice.job->id_ << ".";
    bool finished_job = finished_job_slices == this->config_.general_.num_worker_threads;
    if(finished_job){
        this->finished_job_slices_.erase(job_slice.job->id_);
    }
    return finished_job;
}

void FifoScheduler::Stop() {
    Scheduler::Stop();
    std::unique_lock<std::mutex> lock(this->access_mutex_);
    this->barrier_.Destroy();
    // Set all the current jobs that haven't finished to failed.
    // This will also wakeup any thread waiting on a job.
    for (size_t i = 0; i < this->queue_.size(); i++)
    {
        this->queue_.front()->SetJobStatus(JobStatus::FAILED);
        this->queue_.pop();
    }
    this->undispatched_job_slices_.clear();
    this->finished_job_slices_.clear();
}

} // namespace switchml