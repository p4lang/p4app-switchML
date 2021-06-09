/**
 * SwitchML Project
 * @file rdma_worker_thread.h
 * @brief Declares the RdmaWorkerThread class.
 */

#ifndef SWITCHML_RDMA_WORKER_THREAD_H_
#define SWITCHML_RDMA_WORKER_THREAD_H_

#include <thread>

#include "common.h"
#include "context.h"
#include "config.h"
#include "prepostprocessor.h"
#include "rdma_backend.h"

#ifdef TIMEOUTS
#include "rdma_timeout_queue.h"
#endif

namespace switchml {

/**
 * @brief A class that represents a single rdma worker thread.
 * 
 * A worker thread constantly asks the context for work and carries it out.
 * 
 * Multiple instances of this class is typically created depending on
 * the number of threads in the configuration.
 */
class RdmaWorkerThread {
  public:
    /**
     * @brief Construct a new RDMA Worker Thread object
     * 
     * @param [in] context a reference to the switchml context.
     * @param [in] backend a reference to the created rdma backend.
     * @param [in] config a reference to the context configuration.
     */
    RdmaWorkerThread(Context& context, RdmaBackend& backend, Config& config);

    ~RdmaWorkerThread();

    RdmaWorkerThread(RdmaWorkerThread const&) = default;
    void operator=(RdmaWorkerThread const&) = delete;

    RdmaWorkerThread(RdmaWorkerThread&&) = default;
    RdmaWorkerThread& operator=(RdmaWorkerThread&&) = default;

    /**
     * @brief This is the point of entry function for the thread.
     */
    void operator()();

    /**
     * @brief Start the thread
     */
    void Start();

    /**
     * @brief Wait for the thread to exit and delete its system reference
     */
    void Join();

    /** Worker thread id */
    const WorkerTid tid_;
  private:
    /**
     * @brief Use the backend connection to post a receive work request.
     * 
     * @param qpn The qeueu pair number to post the receive work request for.
     */
    void PostRecvWr(uint16_t qpn);

    /**
     * @brief Setup the next message for the given queue pair to be ready for sending, then use
     * the backend connection to post the send work request so that the message is sent.
     * 
     * This also pushes a timeout entry into the timeout queue for this queue pair or 
     * this outstanding message.
     * 
     * @param qpn The qeueu pair number to post the send work request for.
     */
    void PostSendWr(uint16_t qpn);

    /** Monotonically increasing counter to give unique IDs for each new worker thread **/
    static WorkerTid next_tid_;

    /** A reference to the context */
    Context& context_;
    /** A reference to the context backend */
    RdmaBackend& backend_;
    /** A reference to the context configuration */
    Config& config_;

    /** A pointer to the actual system thread object */
    std::thread* thread_;

    /** The prepostprocessor used by the worker thread */
    std::shared_ptr<PrePostProcessor> ppp_;

    // Connection
    /** 
     * The queue where we will receive work completions for all queue pairs that belong 
     * to this worker thread. It includes both completions for a successfull transmit or receive.
     */
    ibv_cq* completion_queue_;

    /** 
     * Queue pairs to use for sending and receiving. 
     * The number of queue pairs equals the number of outstanding messages.
     */
    std::vector<ibv_qp*> queue_pairs_;

    /** The scatter gather elements used for the send work requests. */
    std::vector<ibv_sge> send_sges_;

    /** Send work requests */
    std::vector<ibv_send_wr> send_wrs_;

    /** Receive work requests */
    std::vector<ibv_recv_wr> recv_wrs_;

    /** 
     * Keeps track of the next message id to send for each queue pair.
     * 
     * This is used to discard duplicate messages and to compute offsets into tensors.
     * 
     * Note: since we only allocate 16 bits for message id in a message,
     * and for large tensors, the msg_ids_ stored here will differ from the ones
     * sent and received in messages (We will call those short_msg_id). We simply select the first 16 bits of this msg_id
     * when we want to send a message and we want to compare with a received message. However we use the full 64 bit
     * id when we invoke the prepostprocessor so that it can compute the offsets correctly.
     */
    std::vector<uint64_t> msg_ids_;

    /** A pointer to the registered buffer that we copy data to before sending and from after receiving */
    void* registered_buffer_ptr_;

    /** 
     * An array keeping track of how many writes were posted for each queue pair
     * This is needed because we want to occasionally make writes signaled (ie. generate a work completion).
     */
    std::vector<int> write_posted_count_per_qp_;

#ifdef TIMEOUTS
    /** A timeout queue for managing outstanding message timeouts efficiently */
    TimeoutQueue timeouts_queue_;
#endif
};

} // namespace switchml

#endif // SWITCHML_RDMA_WORKER_THREAD_H_