/**
 * SwitchML Project
 * @file scheduler.cc
 * @brief Implements the scheduler factory function and other common scheduler functions
 */

#include "scheduler.h"
#include "fifo_scheduler.h"

#include "common_cc.h"

namespace switchml {

std::unique_ptr<Scheduler> Scheduler::CreateInstance(Config& config) {
    std::string& scheduler = config.general_.scheduler;
    if(scheduler == "fifo"){
        return std::make_unique<FifoScheduler>(config);
    } else {
        LOG(FATAL) << "'" << scheduler << "' is not a valid scheduler";
    }
}

Scheduler::Scheduler(Config& config) : 
    stopped_(false),
    config_(config) 
{
    // Do nothing
};

void Scheduler::Stop() {
    std::unique_lock<std::mutex> lock(this->access_mutex_);
    VLOG(2) << "Waking up waiting threads";
    this->stopped_ = true;
    this->job_submitted_event_.notify_all();
}

} // namespace switchml
