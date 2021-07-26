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
 * @file cpu_exponent_quantizer_ppp.h
 * @brief Declares the CpuExponentQuantizerPPP class.
 */


#ifndef SWITCHML_CPU_EXPONENT_QUANTIZER_H_
#define SWITCHML_CPU_EXPONENT_QUANTIZER_H_

#include "common.h"
#include "job.h"
#include "config.h"
#include "prepostprocessor.h"


namespace switchml {

/**
 * @brief A class that implements the switchml exponent quantization scheme using CPU instructions.
 */
class CpuExponentQuantizerPPP : public PrePostProcessor{
  public:
    /**
     * @brief Calls the super class constructor and initialize this class's members
     * 
     * @param [in] config A reference to the context's configuration.
     * @param [in] worker_thread_id The worker thread that this prepostprocessor belongs to.
     * @param [in] ltu_size The size in bytes of the logical transmission unit used by the backend.
     * @param [in] batch_num_ltus How many LTUs constitute a batch.
     */
    CpuExponentQuantizerPPP(Config& config, WorkerTid worker_tid, Numel ltu_size, Numel batch_num_ltus);

    /**
     * @brief Calls CleanupJobSlice() to make sure that any dynamically allocated memory is released.
     * 
     * @see CleanupJobSlice()
     */
    ~CpuExponentQuantizerPPP();

    CpuExponentQuantizerPPP(CpuExponentQuantizerPPP const&) = delete;
    void operator=(CpuExponentQuantizerPPP const&) = delete;

    CpuExponentQuantizerPPP(CpuExponentQuantizerPPP&&) = default;
    CpuExponentQuantizerPPP& operator=(CpuExponentQuantizerPPP&&) = default;
	
    /**
     * @brief Prepare the prepostprocessor's internal variables for this job slice.
     * 
     * This must be called as soon as the worker thread receives a job slice.
     * 
     * @param [in] job_slice A pointer to the job slice currently being worked on by the worker thread.
     * @return uint64_t the number of transmission units that prepostprocessor will need to be sent and received by the backend.
     * 
     * @see CleanupJobSlice()
     */
    uint64_t SetupJobSlice(JobSlice* job_slice) override;

    /**
     * @brief Check whether the currently running job slice needs an extra batch or not.
     * 
     * @return true if the data type is float32
     * @return false otherwise
     */
    bool NeedsExtraBatch() override;


    /**
     * @brief Preprocess a tensor converting it to switchml's representation and loading it into the backend's buffers.
     * 
     * @param [in] ltu_id The id of the logical transmission unit to be preprocessed within the current job slice. 
     * ltu_id will be used to compute the offset into the job slice ltu_id * ltu_size.
     * @param [out] entries_ptr A pointer to where we will store the quantized payload.
     * @param [out] exponent_ptr A pointer to where we will store the exponent in the packet.
     * 
     * @see PostprocessSingle()
     */
    void PreprocessSingle(uint64_t ltu_id, void* entries_ptr, void* exponent_ptr) override;

    /**
     * @brief Postprocess a tensor converting it to the client's representation and loading it into the client's buffers.
     * 
     * @param [in] ltu_id The id of the logical transmission unit to be preprocessed within the current job slice. 
     * ltu_id will be used to compute the offset into the job slice ltu_id * ltu_size.
     * @param [in] entries_ptr A pointer to where we will read the received payload from.
     * @param [in] exponent_ptr A pointer to where we will read the exponent from.
     * 
     * @see PreprocessSingle()
     */
    void PostprocessSingle(uint64_t ltu_id, void* entries_ptr, void* exponent_ptr) override;

    /**
     * @brief Cleans up all internal structures and release any dynamically allocated memory associated with the job slice.
     * 
     * @see SetupJobSlice()
     */
    void CleanupJobSlice() override;

  private:
    /** A pointer to the currently running job slice */
    JobSlice* job_slice_;

    /** An array of the scaling factors computed from the global exponents received from the switch */
    float* scaling_factors_;

    /** 
     * The total number of LTUs to send for the currently running job slice.
     * (This means it excludes the number of extra batch ltus)
     */
    uint64_t total_main_num_ltus_;

    /**
     * How many LTUs constitute a batch for the currently running job slice.
     * (This means that it could be smaller than batch_max_num_ltus_ if the job slice required
     * a small number of LTUs to be transmitted.)
     */
    uint64_t batch_num_ltus_;
};

} // namespace switchml

#endif // SWITCHML_CPU_EXPONENT_QUANTIZER_H_