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