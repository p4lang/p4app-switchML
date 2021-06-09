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
