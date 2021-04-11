/**
 * SwitchML Project
 * @file bypass_ppp.h
 * @brief Declares and implements the BypassPPP class.
 */


#ifndef BYPASS_PPP_H_
#define BYPASS_PPP_H_

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
     * @param [in] worker_thread_id The worker thread that this prepostprocessor belongs to.
     */
    inline BypassPPP(Config& config, WorkerTid worker_thread_id) : PrePostProcessor(config, worker_thread_id) {};

    ~BypassPPP() = default;

    BypassPPP(BypassPPP const&) = delete;
    void operator=(BypassPPP const&) = delete;

    BypassPPP(BypassPPP&&) = default;
    BypassPPP& operator=(BypassPPP&&) = default;
	
    /**
     * @brief Do nothing
     * 
     * @param job_slice ignored
     * @param total_main_num_packets ignored
     * @param batch_num_pkts ignored
     */
    inline void SetupJobSlice(__attribute__((unused)) JobSlice* job_slice, __attribute__((unused)) uint64_t total_main_num_packets,
                              __attribute__((unused)) uint64_t batch_num_pkts) override {};

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
     * @param exponent_ptr ignored
     */
    inline void PreprocessSingle(__attribute__((unused)) uint64_t pkt_id, __attribute__((unused)) void* entries_ptr,
                                 __attribute__((unused)) void* exponent_ptr) override {};

    /**
     * @brief Do nothing
     * 
     * @param pkt_id ignored
     * @param entries_ptr ignored
     * @param exponent_ptr ignored
     */
    inline void PostprocessSingle(__attribute__((unused)) uint64_t pkt_id, __attribute__((unused)) void* entries_ptr,
                                  __attribute__((unused)) void* exponent_ptr) override {};

    /**
     * @brief Do nothing
     */
    inline void CleanupJobSlice() override {};
};

} // namespace switchml

#endif // BYPASS_PPP_H_