/**
 * SwitchML Project
 * @file context.cc
 * @brief Implements the Context class.
 */

#include "context.h"

#include "common_cc.h"
#include "config.h"
#include "fifo_scheduler.h"
#include "backend.h"

#ifndef VERSION_INFO
#define VERSION_INFO "Error: version info should be set in the makefile."
#endif

namespace switchml {

Context& Context::GetInstance() {
    static Context instance;
    return instance;
}

Context::Context()
    : scheduler_()
    , backend_()
    , config_()
    , stats_()
    , context_state_(ContextState::CREATED)
    , number_of_current_jobs_(0)
    , access_mutex_()
    , all_jobs_finished_event_()
{
    INIT_LOG();
    VLOG(0) << "Compiled at " << __DATE__ << ", " __TIME__  << ".";
    VLOG(0) << "Version info: " << VERSION_INFO;
}

Context::~Context() {
    if(this->context_state_ == ContextState::RUNNING) {
        LOG(WARNING) << "The context stop method was not called explicitly. Calling it now in the context destructor.";
        this->Stop();
    }
}

bool Context::Start(Config* config){
    VLOG(0) << "Starting switchml context.";
    std::unique_lock<std::mutex> lock(this->access_mutex_);
    if(this->context_state_ != ContextState::CREATED) {
        LOG(WARNING) << "Cannot start the context unless its in the CREATED state";
        return false;
    }
    this->context_state_ = ContextState::STARTING;

    // Loading config.
    if(config == NULL) {
        bool config_loaded = this->config_.LoadFromFile();
        if(!config_loaded) { // Use default paths
            LOG(FATAL) << "Could not start the context due to missing configuration.";
        }
    } else {
        this->config_ = *config; // Copy config to the context member.
    }

    // Validate the configuration
    this->config_.Validate();

    this->config_.PrintConfig();

    // Initialize stats
    this->stats_.InitStats(this->config_.general_.num_worker_threads);
    // Create scheduler
    this->scheduler_ = Scheduler::CreateInstance(this->config_);
    // Create backend
    this->backend_ = Backend::CreateInstance(*this, this->config_);

    // We need to set it to running before setting up the worker otherwise worker threads will exit.
    this->context_state_ = ContextState::RUNNING;
    // Initialize backend (Starts all worker threads)
    this->backend_->SetupWorker();

    VLOG(0) << "Switchml context started successfully.";

    return true;
}

void Context::Stop() {
    VLOG(0) << "Stopping switchml context";
    std::unique_lock<std::mutex> lock(this->access_mutex_);
    CHECK(this->context_state_ == ContextState::RUNNING) << "You cannot stop the context except when its in the running state";
    this->context_state_ = ContextState::STOPPING;

    // Stop the scheduler (This wakes any waiting threads)
    this->scheduler_->Stop();
    this->number_of_current_jobs_ = 0; // The scheduler was already stopped and all jobs have been dropped.

    // Cleanup backend
    this->backend_->CleanupWorker();

    // Log stats
    this->stats_.LogStats();

    // Cleanup dynamically allocated state
    this->scheduler_ = 0; // This removes the scheduler's reference from the context therefore deallocating the object.
    this->backend_ = 0; // This removes the backend's reference from the context therefore deallocating the object.

    this->context_state_ = ContextState::STOPPED;

    // In case there are any threads waiting for all jobs we must wake them
    lock.unlock();
    this->all_jobs_finished_event_.notify_all();

    VLOG(0) << "Stopped switchml context";
}

std::shared_ptr<Job> Context::AllReduceAsync(void* in_ptr, void* out_ptr, uint64_t numel, DataType data_type, AllReduceOperation all_reduce_operation) {
    LOG_IF(FATAL, this->context_state_ != ContextState::RUNNING) 
        << "You cannot submit a job to the context unless it is in the running state. Current context state: " << this->context_state_ << ".";

    Tensor tensor;
    tensor.in_ptr = in_ptr;
    tensor.out_ptr = out_ptr;
    tensor.numel = numel;
    tensor.data_type = data_type;
    union ExtraJobInfo extras;
    extras.allreduce_operation = all_reduce_operation;
    std::shared_ptr<Job> job = std::make_shared<Job>(tensor, JobType::ALLREDUCE, extras);
    {
        std::unique_lock<std::mutex> lock(this->access_mutex_);
        this->number_of_current_jobs_++;
    }
    this->scheduler_->EnqueueJob(job);

    this->stats_.IncJobsSubmittedNum();
    this->stats_.AppendJobSubmittedNumel(numel);

    return job;
}

std::shared_ptr<Job> Context::AllReduce(void* in_ptr, void* out_ptr, uint64_t numel, DataType data_type, AllReduceOperation all_reduce_operation) {
    LOG_IF(FATAL, this->context_state_ != ContextState::RUNNING) 
        << "You cannot submit a job to the context unless it is in the running state. Current context state: " << this->context_state_ << ".";

    std::shared_ptr<Job> job = this->AllReduceAsync(in_ptr, out_ptr, numel, data_type, all_reduce_operation);
    job->WaitToComplete();
    return job;
}

void Context::WaitForAllJobs() {
    LOG_IF(FATAL, this->context_state_ != ContextState::RUNNING) 
        << "You cannot wait for all jobs unless the context is in the running state. Current context state: " << this->context_state_ << ".";

    std::unique_lock<std::mutex> lock(this->access_mutex_);
    this->all_jobs_finished_event_.wait(lock, [this] {return !this->number_of_current_jobs_;});
}

bool Context::GetJobSlice(WorkerTid worker_thread_id, JobSlice& job_slice) {
    LOG_IF(FATAL, this->context_state_ != ContextState::RUNNING) 
        << "You cannot get a job slice unless the context is in the running state. Current context state: " << this->context_state_ << ".";
    
    return this->scheduler_->GetJobSlice(worker_thread_id, job_slice);
}

void Context::NotifyJobSliceCompletion(WorkerTid worker_thread_id, const JobSlice& job_slice) {
    LOG_IF(FATAL, this->context_state_ != ContextState::RUNNING) 
        << "You cannot notify job slice completion unless the context is in the running state. Current context state: " << this->context_state_ << ".";

    bool job_finished = this->scheduler_->NotifyJobSliceCompletion(worker_thread_id, job_slice);
    if(job_finished){
        job_slice.job->SetJobStatus(JobStatus::FINISHED);
        std::unique_lock<std::mutex> lock(this->access_mutex_);
        this->number_of_current_jobs_--;
        this->stats_.IncJobsFinishedNum();
        VLOG(1) << "Finished Job with id: " << job_slice.job->id_ << " status: " << job_slice.job->GetJobStatus()
            << ". Currently running jobs: " << this->number_of_current_jobs_ << ".";
        // If current jobs = 0 wake up threads waiting for all jobs;
        if(!this->number_of_current_jobs_) {
            lock.unlock();
            this->all_jobs_finished_event_.notify_all();
        }
    }
}

Context::ContextState Context::GetContextState() {
    return this->context_state_;
}

const Config& Context::GetConfig() {
    return this->config_;
}

Stats& Context::GetStats() {
    return this->stats_;
}

} // namespace switchml