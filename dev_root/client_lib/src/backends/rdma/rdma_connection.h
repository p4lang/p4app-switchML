/**
 * SwitchML Project
 * @file rdma_connection.h
 * @brief Declares the RdmaConnection class.
 */

#ifndef SWITCHML_RDMA_CONNECTION_H_
#define SWITCHML_RDMA_CONNECTION_H_

#include <infiniband/verbs.h>

#include <vector>

#include "common.h"
#include "config.h"
#include "rdma_endpoint.h"
#include "rdma_grpc_client.h"

namespace switchml {

/**
 * @brief The RdmaConnection represents the connection to both the controller and the switch.
 * 
 * It is used by the backend to setup the connection by exchanging the needed information with the controller via
 * the GrpcClient, sets up and brings up queue pairs, and finally worker threads then use it to send and
 * receive messages.
 */
class RdmaConnection {
  public:

    /**
     * @brief Post receive work request and check its success.
     * 
     * @param [in] qp The queue pair to use.
     * @param [in] wr The receive work request to post
     */
    static void PostRecv(ibv_qp* qp, ibv_recv_wr* wr);

    /**
     * @brief Post send work request and check its success.
     * 
     * @param [in] qp The queue pair to use
     * @param [in] wr The send work request to post.
     */
    static void PostSend(ibv_qp* qp, ibv_send_wr* wr);

    /**
     * @brief Initialize all members and allocate buffer memory region.
     * 
     * @param [in] config a reference to the switchml configuration. 
     */
    RdmaConnection(Config& config);

    ~RdmaConnection();

    RdmaConnection(RdmaConnection const&) = delete;
    void operator=(RdmaConnection const&) = delete;

    RdmaConnection(RdmaConnection&&) = default;
    RdmaConnection& operator=(RdmaConnection&&) = default;

    /**
     * @brief Performs all needed setup and bringup to establish the RDMA connection.
     * 
     * This should be the first function to be called after creating the object.
     * After calling this function, you can go ahead and use the getters to 
     * access the created queue pairs, memory region and so on.
     * You can also then use the PostSend() and PostRecv() functions to send
     * and receive messages.
     */
    void Connect();

    // Getters
    ibv_cq* GetWorkerThreadCompletionQueue(WorkerTid worker_thread_id);

    /**
     * @brief Get the range of queue pairs corresponding to a worker thread.
     * 
     * @param [in] worker_thread_id
     * @return std::vector<ibv_qp*> 
     */
    std::vector<ibv_qp*> GetWorkerThreadQueuePairs(WorkerTid worker_thread_id);

    /**
     * @brief Get the range of rkeys corresponding to a worker thread.
     * 
     * @param worker_thread_id 
     * @return std::vector<uint32_t> 
     */
    std::vector<uint32_t> GetWorkerThreadRkeys(WorkerTid worker_thread_id);

    /**
     * @brief Get the memory region information corresponding to a worker thread.
     * 
     * @param worker_thread_id 
     * @return std::pair<void*, uint32_t> first element is the first address in the 
     * memory region that the worker thread can access. Second element is the lkey of 
     * the memory region.
     */
    std::pair<void*, uint32_t> GetWorkerThreadMemoryRegion(WorkerTid worker_thread_id);

    /**
     * @brief Get the underlying used endpoint
     * 
     * @return RdmaEndpoint& 
     */
    RdmaEndpoint& GetEndpoint();

  private:
    /**
     * @brief Create and initialize all queue pairs.
     */
    void InitializeQueuePairs();

    /**
     * @brief Exchange queue pair and memory region information with 
     * the controller program
     */
    void ExchangeConnectionInfo();

    /**
     * @brief Brings up all queue pairs up to the ready to send state.
     * 
     * This function calls MoveToRtr(), and MoveToRts()
     */
    void ConnectQueuePairs();

    /**
     * @brief Move all queue pairs to the init state.
     * 
     * @param [in] qp_index the index of the queue pair in the
     * queue_pairs_, neighbor_qpns_, neighbor_psns_, neighbor_rkeys_ vectors.
     */
    void MoveToInit(int qp_index);

    /**
     * @brief Move all queue pairs to the ready to receive state.
     * 
     * @param [in] qp_index the index of the queue pair in the
     * queue_pairs_, neighbor_qpns_, neighbor_psns_, neighbor_rkeys_ vectors.
     */
    void MoveToRtr(int qp_index);

    /**
     * @brief Move all queue pairs to the ready to send state.
     * 
     * @param [in] qp_index the index of the queue pair in the
     * queue_pairs_, neighbor_qpns_, neighbor_psns_, neighbor_rkeys_ vectors.
     */
    void MoveToRts(int qp_index);

    // Constants from Mellanox RDMA-Aware Programming manual
    const int kMinRnrTimer = 0x12;
    const int kTimeout = 14;
    const int kRetryCount = 7;
    const int kRnrRetry = 7;
    const int kMaxDestRdAtomic = 0;
    const int kMaxRdAtomic = 0;

    /** A reference to the context configuration */
    Config& config_;

    /** The underlying endpoint used */
    RdmaEndpoint endpoint_;

    /** The GRPC client which will talk to the controller program */
    RdmaGrpcClient grpc_client_;

    /** The registered memory region used as an intermediate buffer */
    ibv_mr* memory_region_;

    const ibv_mtu mtu_;
    const uint32_t num_queue_pairs_;

    std::vector<ibv_cq*> completion_queues_;

    std::vector<ibv_qp*> queue_pairs_;

    // Data about neighbor queue pairs
    std::vector<ibv_gid> neighbor_gids_;
    std::vector<uint32_t> neighbor_qpns_;
    std::vector<uint32_t> neighbor_psns_;
    std::vector<uint32_t> neighbor_rkeys_;
};

} // namespace switchml

#endif // SWITCHML_RDMA_CONNECTION_H_