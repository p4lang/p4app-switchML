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
    VLOG(1) << "Waking up waiting threads";
    this->stopped_ = true;
    this->job_submitted_event_.notify_all();
}

} // namespace switchml
