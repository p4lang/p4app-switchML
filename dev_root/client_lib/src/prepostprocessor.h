/**
 * SwitchML Project
 * @file prepostprocessor.h
 * @brief Declares the PrePostProcessor interface.
 */


#ifndef SWITCHML_PREPOSTPROCESSOR_H_
#define SWITCHML_PREPOSTPROCESSOR_H_

#include "common.h"
#include "job.h"
#include "config.h"

namespace switchml {

/**
 * @brief A PrePostProcessor (PPP) is an object that handles loading and unloading of the data between the client and the network.
 * 
 * Depending on the implementation, the PPP may convert the representation of the data, and may require extra information
 * or metadata to be sent so that it can undo its representation changes.
 * 
 * In the prepostprocessor we use 'packet' to refer to the unit of transmission that the backend will use.
 * But the prepostprocessor itself does not care what that "packet" really is. Its just dealing with a 
 * series of blocks of data that is being sent and received. Call it a packet, a block, a message.
 * 
 * At this time the prepostprocessor cannot change the size of the data to be sent but this is something that
 * we may implement in the future by changing the job_slice fields that since its passed by pointer.
 */
class PrePostProcessor {
  public:
    /**
     * @brief Create an instance of the prepostprocessor specified in the configuration passed.
     * 
     * @param [in] config A reference to the context's configuration.
     * @param [in] worker_thread_id The worker thread that this prepostprocessor belongs to.
     * @return std::shared_ptr<PrePostProcessor> a shared pointer to the prepostprocessor's created instance.
     */
    static std::shared_ptr<PrePostProcessor> CreateInstance(Config& config, WorkerTid worker_thread_id);

    ~PrePostProcessor() = default;

    PrePostProcessor(PrePostProcessor const&) = delete;
    void operator=(PrePostProcessor const&) = delete;

    PrePostProcessor(PrePostProcessor&&) = default;
    PrePostProcessor& operator=(PrePostProcessor&&) = default;

    /**
     * @brief Setup the PPP's internal structures and prepare to start processing the passed job slice.
     * 
     * @param [in] job_slice A pointer to the job slice currently being worked on by the worker thread.
     * @param [in] total_main_num_pkts The total number of main packets that will be sent.
     * @param [in] batch_num_pkts The number of packets in a batch. This could be equal to or less than max_outstanding_pkts.
     */
		virtual void SetupJobSlice(JobSlice* job_slice, uint64_t total_main_num_pkts, uint64_t batch_num_pkts) = 0;

    /**
     * @brief Check whether this prepostprocessor needs to send an extra batch for the current job slice or not.
     * 
     * Some prepostprocessor's need extra info / metadata to be sent along the payload so that they convert between
     * representations correctly. And they usually need that extra info to be present before the first real batch
     * is sent. In that case the backend sends an extra first batch to make this information available for the first real batch later.
     * 
     * @return true If the prepostprocessor needs an extra batch
     * @return false If it doesn't
     */
    virtual bool NeedsExtraBatch() = 0;

    /**
     * @brief Preprocess a packet converting its representation if needed and moving its payload into the backend's buffers.
     * 
     * @param [in] pkt_id The id of the packet to be preprocessed within the current job slice. pkt_id = offset into the job slice / number of max elements in a packet.
     * @param [out] entries_ptr A pointer to where we will store the payload.
     * @param [out] extra_info A pointer to where we will store the extra info if we need it.
     * 
     * @see PostprocessSingle()
     */
		virtual void PreprocessSingle(uint64_t pkt_id, void* entries_ptr, void* extra_info = nullptr) = 0;

    /**
     * @brief Postprocess a packet converting it to the original representation if needed and moving its payload into the client's buffers.
     * 
     * @param [in] pkt_id The id of the packet to be preprocessed within the current job slice. pkt_id = offset into the job slice / number of max elements in a packet.
     * @param [in] entries_ptr A pointer to where we read the payload from
     * @param [in] extra_info A pointer to where we will read the extra info from if we need it.
     * 
     * @see PreprocessSingle()
     */
		virtual void PostprocessSingle(uint64_t pkt_id, void* entries_ptr, void* extra_info = nullptr) = 0;

		// virtual void PrePostprocessSingle(uint64_t pre_id, Tensor pre_tensor, void* pre_extra_info, uint64_t post_id, Tensor post_tensor, void* post_extra_info) = 0;
		
		// virtual void PreprocessBulk(uint64_t pkt_id, Tensor tensor, void** extra_info = nullptr) = 0;
		
		// virtual void PostprocessBulk(uint64_t pkt_id, Tensor tensor, void** extra_info = nullptr) = 0;

    /**
     * @brief Cleans up all internal structures and released any dynamically allocated memory associated with the job slice.
     * 
     * @see SetupJobSlice()
     */
    virtual void CleanupJobSlice() = 0;

  protected:
    /**
     * @brief Construct a new PrePostProcessor object
     * 
     * @param [in] config A reference to the context configuration
     * @param [in] worker_thread_id The worker thread that this prepostprocessor belongs to
     */
    PrePostProcessor(Config& config, WorkerTid worker_thread_id);

    /** A reference to the context configuration */
    Config& config_;

    /** The worker thread that this prepostprocessor belongs to */
    WorkerTid worker_tid_;
};

} // namespace switchml

#endif // SWITCHML_PREPOSTPROCESSOR_H_