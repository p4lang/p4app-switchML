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

std::shared_ptr<PrePostProcessor> PrePostProcessor::CreateInstance(Config& config, WorkerTid worker_tid) {
    std::string& ppp = config.general_.prepostprocessor;
    if(ppp == "cpu_exponent_quantizer") {
        return std::make_shared<CpuExponentQuantizerPPP>(config, worker_tid);
    } else if (ppp == "bypass") {
        return std::make_shared<BypassPPP>(config, worker_tid);
    } else {
        LOG(FATAL) << "'" << ppp << "' is not a valid prepostprocessor.";
    }
}

PrePostProcessor::PrePostProcessor(Config& config, WorkerTid worker_tid) :
    config_(config),
    worker_tid_(worker_tid)
{
    // Do nothing
}

} // namespace switchml
