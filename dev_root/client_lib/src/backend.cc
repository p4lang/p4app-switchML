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
 * @file backend.cc
 * @brief Implements the backend factory function and the constructors/destructors.
 * 
 * This is where choosing between the different backends happens.
 */

#include "backend.h"

#include "common_cc.h"
#ifdef DUMMY
#include "dummy_backend.h"
#endif
#ifdef RDMA
#include "rdma_backend.h"
#endif
#ifdef DPDK
#include "dpdk_backend.h"
#endif

namespace switchml {

std::unique_ptr<Backend> Backend::CreateInstance(Context& context, Config& config) {
    std::string& backend = config.general_.backend;
    if(backend == "dummy") {
#ifdef DUMMY
        return std::make_unique<DummyBackend>(context, config);
#else
        LOG(FATAL) << "SwitchML was not compiled with '" << backend << "' backend support.";
#endif
    }
    if (backend == "rdma") {
#ifdef RDMA
        return std::make_unique<RdmaBackend>(context, config);
#else
        LOG(FATAL) << "SwitchML was not compiled with '" << backend << "' backend support.";
#endif
    }
    if (backend == "dpdk") {
#ifdef DPDK
        return std::make_unique<DpdkBackend>(context, config);
#else
        LOG(FATAL) << "SwitchML was not compiled with '" << backend << "' backend support.";
#endif
    }
    LOG(FATAL) << "'" << backend << "' is not a valid backend.";
}

Backend::Backend(Context& context, Config& config) : 
    context_(context),
    config_(config) 
{
    // Do nothing
}

} // namespace switchml