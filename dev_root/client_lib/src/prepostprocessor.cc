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
 * @file prepostprocessor.cc
 * @brief Implements the prepostprocessor factory function and the constructors/destructors.
 */

#include "prepostprocessor.h"

#include <memory>

#include "cpu_exponent_quantizer_ppp.h"
#include "bypass_ppp.h"

namespace switchml {

std::shared_ptr<PrePostProcessor> PrePostProcessor::CreateInstance(Config& config, WorkerTid worker_tid, Numel ltu_size, Numel batch_num_ltus) {
    std::string& ppp = config.general_.prepostprocessor;
    if(ppp == "cpu_exponent_quantizer") {
        return std::make_shared<CpuExponentQuantizerPPP>(config, worker_tid, ltu_size, batch_num_ltus);
    } else if (ppp == "bypass") {
        return std::make_shared<BypassPPP>(config, worker_tid, ltu_size, batch_num_ltus);
    } else {
        LOG(FATAL) << "'" << ppp << "' is not a valid prepostprocessor.";
    }
}

PrePostProcessor::PrePostProcessor(Config& config, WorkerTid worker_tid, Numel ltu_size, Numel batch_num_ltus) :
    config_(config),
    worker_tid_(worker_tid),
    ltu_size_(ltu_size),
    batch_max_num_ltus_(batch_num_ltus)
{
    // Do nothing
}

} // namespace switchml
