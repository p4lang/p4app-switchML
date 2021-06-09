/**
 * SwitchML Project
 * @file rdma_endpoint.h
 * @brief Declares the RdmaEndpoint class.
 */

#ifndef SWITCHML_RDMA_ENDPOINT_H_
#define SWITCHML_RDMA_ENDPOINT_H_

#include <infiniband/verbs.h>
#include <string>

#include "common.h"

namespace switchml {

/**
 * @brief The RdmaEndpoint class contains all functions and configurations
 * to setup the machine and the device/NIC.
 * 
 * It is mainly used by the RdmaConnection class.
 */
class RdmaEndpoint {
  public:

    /**
     * @brief Initialize members and configure and open the ibverbs device port.
     * 
     * @param device_name The name of the Infiniband device to use.
     * @param device_port_id The specific port to use from the device. 
     * @param gid_index The GID index to use.
     */
    RdmaEndpoint(std::string device_name, uint16_t device_port_id, uint16_t gid_index);

    ~RdmaEndpoint();

    RdmaEndpoint(RdmaEndpoint const&) = delete;
    void operator=(RdmaEndpoint const&) = delete;

    RdmaEndpoint(RdmaEndpoint&&) = default;
    RdmaEndpoint& operator=(RdmaEndpoint&&) = default;

    /**
     * @brief Allocates and registers a memory region at a specific address.
     * 
     * The call will fail if the allocation is not possible.
     * 
     * @param requested_address The memory address wanted.
     * @param size The size of the region in bytes.
     * @return ibv_mr* A pointer to the allocated memory region struct.
     */
    ibv_mr* AllocateAtAddress(void* requested_address, uint64_t size);

    /**
     * @brief Free a memory region that was allocated and registered.
     * 
     * @param mr The memory region to free.
     */
    void free(ibv_mr* mr);

    /**
     * @brief Create a Completion Queue.
     * 
     * @return ibv_cq* The created completion queue.
     */
    ibv_cq* CreateCompletionQueue();

    /**
     * @brief Create a Queue Pair.
     * 
     * @param completion_queue The completion queue to associate with this queue pair.
     * @return ibv_qp* The created queue pair.
     */
    ibv_qp* CreateQueuePair(ibv_cq* completion_queue);

    // Getters

    /**
     * @brief Get the MAC address corresponding to the chosen GID.
     * 
     * @return uint64_t The MAC address.
     */
    uint64_t GetMac();

    /**
     * @brief Get the IPv4 address corresponding to the chosen GID.
     * 
     * @return uint32_t The IPv4 address
     */
    uint32_t GetIPv4();

    ibv_port_attr GetPortAttributes();

    ibv_device* GetDevice();

  private:
    // Constants for initializing queues

    /** 
     * How many completions can the completion queue hold. 
     * Must accomodate both send and receive completions.
     * Thus it should be at least kSendQueueDepth + kReceiveQueueDepth
    */
    const int kCompletionQueueDepth = 4096;

    /** How many send requests can be in the send queue at a time */
    const int kSendQueueDepth = 2048;

    /** How many receive notifications can be in the receive queue a time. */
    const int kReceiveQueueDepth = 2048;

    /** */
    const int kScatterGatherElementCount = 1;

    /** */
    const int kMaxInlineData = 16;

    // Data members

    /** Handle to the chosen Infiniband device. */
    ibv_device* device_;

    /** The index of the chosen port within the device. */
    uint16_t device_port_id_;

    /** A struct describing all of the chosen port's attributes */
    ibv_port_attr port_attributes_;

    /**
     * The GID index of the chosen port:
     * 0: RoCEv1 with MAC-based GID, 1:RoCEv2 with MAC-based GID,
     * 2: RoCEv1 with IP-based GID, 3: RoCEv2 with IP-based GID
     */
    uint16_t gid_index_; 

    /** The chosen port's GID. */
    ibv_gid gid_;

    /** The device context. Used for most Verbs operations */
    ibv_context* context_;

    /** The protection domain to go with the context */
    ibv_pd* protection_domain_;

    /** How many CPU ticks per second. (Used to efficiently measure time) */
    uint64_t ticks_per_sec_;
};

} // namespace switchml
#endif // SWITCHML_RDMA_ENDPOINT_H_