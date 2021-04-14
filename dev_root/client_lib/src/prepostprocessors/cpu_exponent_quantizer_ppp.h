/**
 * SwitchML Project
 * @file cpu_exponent_quantizer.h
 * @brief Declares the CpuExponentQuantizerPPP class.
 */


#ifndef CPU_EXPONENT_QUANTIZER_H_
#define CPU_EXPONENT_QUANTIZER_H_

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
     */
    CpuExponentQuantizerPPP(Config& config, WorkerTid worker_thread_id);

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
     * @param [in] job_slice The job slice that the worker thread received.
     * @param [in] total_main_num_pkts The total number of main packets to send for the currently running job slice
     * This means it excludes the number of extra batch packets.
     * @param [in] batch_num_pkts How many packets will be in a batch.
     * @see CleanupJobSlice()
     */
    void SetupJobSlice(JobSlice* job_slice, uint64_t total_main_num_pkts, uint64_t batch_num_pkts) override;

    /**
     * @brief Check whether the currently running job slice needs an extra batch or not.
     * 
     * This must be called only after calling SetupJobSlice() to setup the member pointer
     * to the job_slice correctly.
     * 
     * @return true if the data type is float32
     * @return false otherwise
     */
    bool NeedsExtraBatch() override;

    /**
     * @brief Preprocess a tensor converting it to switchml's representation and loading it into the backend's buffers.
     * 
     * @param [in] pkt_id The id of the packet to be preprocessed within the current job slice. pkt_id = offset into the job slice / number of max elements in a packet.
     * @param [out] entries_ptr A pointer to where we will store the quantized payload.
     * @param [out] exponent_ptr A pointer to where we will store the exponent in the packet.
     * 
     * @see PostprocessSingle()
     */
    void PreprocessSingle(uint64_t pkt_id, void* entries_ptr, void* exponent_ptr) override;

    /**
     * @brief Postprocess a tensor converting it to the client's representation and loading it into the client's buffers.
     * 
     * @param [in] pkt_id The id of the packet to be preprocessed within the current job slice. pkt_id = offset into the job slice / number of max elements in a packet.
     * @param [in] entries_ptr A pointer to where we will read the recevied payload from.
     * @param [in] exponent_ptr A pointer to where we will read the exponent from.
     * 
     * @see PreprocessSingle()
     */
    void PostprocessSingle(uint64_t pkt_id, void* entries_ptr, void* exponent_ptr) override;

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

    /** The batch size for the currently running job slice */
    uint64_t batch_num_pkts_;

    /** 
     * The total number of main packets to send for the currently running job slice
     * (This means it excludes the number of extra batch packets)
     */
    uint64_t total_main_num_pkts_;
};

} // namespace switchml

#endif // CPU_EXPONENT_QUANTIZER_H_