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
 * @file bypass_ppp.h
 * @brief Declares and implements the BypassPPP class.
 */


#ifndef SWITCHML_BYPASS_PPP_H_
#define SWITCHML_BYPASS_PPP_H_

#include "common.h"
#include "job.h"
#include "config.h"
#include "prepostprocessor.h"

namespace switchml {

/**
 * @brief A class that ignores prepostprocessing completely and just serves as a placeholder.
 * 
 * It is used for debugging and measuring performance without any prepostprocessing.
 * It consists of mostly empty inline functions that will most likely be simply compiled away.
 */
class BypassPPP : public PrePostProcessor{
  public:
    /**
     * @brief Calls the super class constructor.
     * 
     * @param [in] config A reference to the context's configuration.
     * @param [in] worker_tid The worker thread that this prepostprocessor belongs to.
     * @param [in] ltu_size The size in bytes of the logical transmission unit used by the backend.
     * @param [in] batch_num_ltus How many LTUs constitute a batch.
     */
    inline BypassPPP(Config& config, WorkerTid worker_tid, Numel ltu_size, Numel batch_num_ltus) : 
                     PrePostProcessor(config, worker_tid, ltu_size, batch_num_ltus) {}

    ~BypassPPP() = default;

    BypassPPP(BypassPPP const&) = delete;
    void operator=(BypassPPP const&) = delete;

    BypassPPP(BypassPPP&&) = default;
    BypassPPP& operator=(BypassPPP&&) = default;
	
    /**
     * @brief Compute the number of LTUs needed
     * 
     * @param [in] job_slice A pointer to the job slice currently being worked on by the worker thread.
     * @return uint64_t the number of transmission units that prepostprocessor will need to be sent and received by the backend.
     */
    inline uint64_t SetupJobSlice(JobSlice* job_slice) override {
        uint64_t tensor_size = job_slice->slice.numel * DataTypeSize(job_slice->slice.data_type);
        uint64_t total_num_ltus = (tensor_size + this->ltu_size_ - 1) / this->ltu_size_; // Roundup division
        return total_num_ltus;
    }

    /**
     * @brief always return false
     * 
     * @return true Never
     * @return false Always
     */
    inline bool NeedsExtraBatch() override { return false; };

    /**
     * @brief Do nothing
     * 
     * @param pkt_id ignored
     * @param entries_ptr ignored
     * @param extra_info ignored
     */
    inline void PreprocessSingle(__attribute__((unused)) uint64_t pkt_id, __attribute__((unused)) void* entries_ptr,
                                 __attribute__((unused)) void* extra_info) override {}

    /**
     * @brief Do nothing
     * 
     * @param pkt_id ignored
     * @param entries_ptr ignored
     * @param extra_info ignored
     */
    inline void PostprocessSingle(__attribute__((unused)) uint64_t pkt_id, __attribute__((unused)) void* entries_ptr,
                                  __attribute__((unused)) void* extra_info) override {}

    /**
     * @brief Do nothing
     */
    inline void CleanupJobSlice() override {}
};

} // namespace switchml

#endif // SWITCHML_BYPASS_PPP_H_