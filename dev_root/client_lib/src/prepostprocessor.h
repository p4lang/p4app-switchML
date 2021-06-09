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
 * Depending on the implementation, the PPP may convert the representation of the data (maybe even compress it) and may require extra information
 * or metadata to be sent so that it can undo its representation changes.
 * 
 * In the prepostprocessor we use 'LTU' to refer to the logical unit of transmission that the backend will use.
 * But the prepostprocessor itself does not care what that "logical transmission unit" really is. Its just dealing with a 
 * series of blocks of data that is being sent and received. Call it a packet (for dpdk), a block, a message (in rdma).
 * 
 */
class PrePostProcessor {
  public:
    /**
     * @brief Create an instance of the prepostprocessor specified in the configuration passed.
     * 
     * @param [in] config A reference to the context's configuration.
     * @param [in] worker_tid The worker thread that this prepostprocessor belongs to.
     * @param [in] ltu_size The size in bytes of the logical transmission unit used by the backend.
     * @param [in] batch_num_ltus How many LTUs constitute a batch.
     * @return std::shared_ptr<PrePostProcessor> a shared pointer to the prepostprocessor's created instance.
     */
    static std::shared_ptr<PrePostProcessor> CreateInstance(Config& config, WorkerTid worker_tid, Numel ltu_size, Numel batch_num_ltus);

    ~PrePostProcessor() = default;

    PrePostProcessor(PrePostProcessor const&) = delete;
    void operator=(PrePostProcessor const&) = delete;

    PrePostProcessor(PrePostProcessor&&) = default;
    PrePostProcessor& operator=(PrePostProcessor&&) = default;

    /**
     * @brief Setup the PPP's internal structures and prepare to start processing the passed job slice.
     * 
     * @param [in] job_slice A pointer to the job slice currently being worked on by the worker thread.
     * @return uint64_t the number of transmission units that prepostprocessor will need to be sent and received by the backend so
     * that the whole tensor is processed. (This does not include LTUs from the extra batch that might be needed).
     * We let the PPP return this information so that the backend is aware in case the PPP reduces the size of the data
     * and thus needs a smaller number of LTUs to be transmitted.
     */
		virtual uint64_t SetupJobSlice(JobSlice* job_slice) = 0;

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
     * @brief Preprocess an LTU converting its representation if needed and moving its payload into the backend's buffers.
     * 
     * @param [in] ltu_id The id of the logical transmission unit to be preprocessed within the current job slice. 
     * ltu_id will be used to compute the offset into the job slice ltu_id * ltu_size.
     * @param [out] entries_ptr A pointer to where we will store the payload to be ready for transmission.
     * @param [out] extra_info A pointer to where we will store the extra info if we need it.
     * 
     * @see PostprocessSingle()
     */
		virtual void PreprocessSingle(uint64_t ltu_id, void* entries_ptr, void* extra_info = nullptr) = 0;

    /**
     * @brief Postprocess an LTU converting it to the original representation if needed and moving its payload into the client's buffers.
     * 
     * @param [in] ltu_id The id of the logical transmission unit to be postprocessed within the current job slice.
     * ltu_id will be used to compute the offset into the job slice ltu_id * ltu_size.
     * @param [in] entries_ptr A pointer to where we will read the received payload from.
     * @param [in] extra_info A pointer to where we will read the extra info from if we need it.
     * 
     * @see PreprocessSingle()
     */
		virtual void PostprocessSingle(uint64_t ltu_id, void* entries_ptr, void* extra_info = nullptr) = 0;

		// virtual void PrePostprocessSingle(...) = 0;
		
		// virtual void PreprocessBulk(...) = 0;
		
		// virtual void PostprocessBulk(...) = 0;

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
     * @param [in] worker_tid The worker thread that this prepostprocessor belongs to
     * @param [in] ltu_size The size in bytes of the logical transmission unit used by the backend.
     * @param [in] batch_num_ltus How many LTUs constitute a batch.
     */
    PrePostProcessor(Config& config, WorkerTid worker_tid, Numel ltu_size, Numel batch_num_ltus);

    /** A reference to the context configuration */
    Config& config_;

    /** The worker thread that this prepostprocessor belongs to */
    WorkerTid worker_tid_;

    /** The size in bytes of the logical transmission unit used by the backend. */
    Numel ltu_size_;

    /** What's the maximum number of LTUs that can constitute a batch. */
    Numel batch_max_num_ltus_;
};

} // namespace switchml

#endif // SWITCHML_PREPOSTPROCESSOR_H_