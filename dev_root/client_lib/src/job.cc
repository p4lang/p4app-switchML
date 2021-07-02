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
 * @file job.cc
 * @brief Implements the Job class.
 */

#include "job.h"

#include "common_cc.h"

namespace switchml {

JobId Job::next_id_ = 0;

Job::Job(Tensor tensor, JobType job_type, ExtraJobInfo extra_job_info) :
 id_(next_id_), tensor_(tensor), job_type_(job_type), extra_job_info_(extra_job_info),
 job_status_(JobStatus::INIT) {
     Job::next_id_++;
}

void Job::WaitToComplete() {
    std::unique_lock<std::mutex> lock(this->access_mutex_);

    this->job_finished_event_.wait(lock);
}

JobStatus Job::GetJobStatus() {
    return this->job_status_;
}

void Job::SetJobStatus(JobStatus job_status) {
    std::unique_lock<std::mutex> lock(this->access_mutex_);
    LOG_IF(FATAL, job_status < this->job_status_) << "Illegal change of job status. You cannot change job status from '" << this->job_status_ << "' to '" << job_status << "'";
    this->job_status_ = job_status;
    if(this->job_status_ == JobStatus::FAILED || this->job_status_ == JobStatus::FINISHED) {
        lock.unlock();
        this->job_finished_event_.notify_all();
    }
}

} // namespace switchml