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
 * @file rdma_backend.cc
 * @brief Declares the RdmaBackend class.
 */

#include "rdma_backend.h"

#include "common_cc.h"
#include "rdma_worker_thread.h"

namespace switchml {

RdmaBackend::RdmaBackend(Context& context, Config& config)
    : Backend(context, config)
    , worker_threads_()
{
    // Do nothing
}

RdmaBackend::~RdmaBackend() {
    // Do nothing
}

void RdmaBackend::SetupWorker() {
    VLOG(0) << "Setting up worker.";

    // Create RDMA connection
    this->connection_ = std::make_unique<RdmaConnection>(this->config_);
    this->connection_->Connect();

    // Start worker threads
    for(int i = 0; i < this->config_.general_.num_worker_threads; i++) {
        this->worker_threads_.push_back(RdmaWorkerThread(this->context_,*this, this->config_));
        this->worker_threads_[i].Start();
    }
}

void RdmaBackend::CleanupWorker() {
    VLOG(0) << "Cleaning up worker.";

    // Wait for all worker threads to exit.
    for(int i = 0; i < this->config_.general_.num_worker_threads; i++) {
        this->worker_threads_[i].Join();
    }
}

std::unique_ptr<RdmaConnection>& RdmaBackend::GetConnection() {
    return this->connection_;
}

} // namespace switchml