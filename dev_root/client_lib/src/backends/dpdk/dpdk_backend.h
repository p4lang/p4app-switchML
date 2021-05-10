/**
 * SwitchML Project
 * @file dpdk_backend.h
 * @brief Declares the DpdkBackend class.
 */

#ifndef SWITCHML_DPDK_BACKEND_H_
#define SWITCHML_DPDK_BACKEND_H_

#ifndef DPDK
#define DPDK // Just so that IDEs activate the dpdk sections of the code
#endif

#include <memory>

#include <rte_ethdev.h>

#include "common.h"
#include "context.h"
#include "backend.h"

namespace switchml {

// Forward declare thread classes to be able to add them as member variables
class DpdkWorkerThread;
class DpdkMasterThread;

/**
 * @brief The backend that represents the dpdk version of switchml.
 */
class DpdkBackend : public switchml::Backend {
  public:

    /**
     * @brief The switchml dpdk packet header.
     */
    struct DpdkPacketHdr {
        /**
         * This field is used to store both the job type and the packet's size enum or category.
         * The 4 MSBs are for the job type and the 4 LSBs are for the size.
         */
        uint8_t job_type_size;

        /** 
         * The 8 LSBs of the id of the job associated with this packet.
         * This is used by the client only to discard duplicates at the edge of switching from one job to another.
         * Therefore we do not need the full length of the job id.
         */
        uint8_t short_job_id;

        /** An id to identify a packet within a job slice. This is used by the client only. */
        uint32_t pkt_id;

        /** 
         * The switch's pool/slot index.
         * 
         * A pool or a slot in the switch is what is used to store the values of a packet.
         * Think of the switch as a large array of pools or slots. Each packet sent addresses
         * a particular pool/slot.
         * 
         * The MSB of this field is used to alternate between two sets of pools/slots
         * to have a shadow copy for switch retransmissions.
        */
        uint16_t switch_pool_index; 
    }__attribute__((__packed__));

    /** A type representing a single element in the packet */
    typedef int32_t DpdkPacketElement;

    /**
     * @brief A struct to store an end to end 
     * network address.
     */
    struct E2eAddress {
      uint8_t mac[6];
      uint32_t ip;
      uint16_t port;
    };

    /**
     * @brief Call the super class constructor.
     * 
     * @param [in] context The context
     * @param [in] config The context configuration.
     */
    DpdkBackend(Context& context, Config& config);

    ~DpdkBackend();

    DpdkBackend(DpdkBackend const&) = delete;
    void operator=(DpdkBackend const&) = delete;

    DpdkBackend(DpdkBackend&&) = default;
    DpdkBackend& operator=(DpdkBackend&&) = default;

    /**
     * @brief Creates and starts the dpdk master thread.
     * 
     * Which in turn initializes the DPDK EAL and creates and starts all worker threads.
     * @see DpdkMasterThread::operator()()
     * @see DpdkWorkerThread::operator()()
     * @see CleanupWorker()
     */
    void SetupWorker() override;

    /**
     * @brief Waits for the dpdk master thread to exit.
     * @see SetupWorker()
     */
    void CleanupWorker() override;

    /**
     * @brief Get a reference to the switch end to end address in big endian.
     * 
     * The reference can be used to modify the address.
     * 
     * @return struct E2eAddress& a reference to the address object.
     */
    struct E2eAddress& GetSwitchE2eAddr();

    /**
     * @brief Get a reference to the worker end to end address in big endian.
     * 
     * The reference can be used to modify the address.
     * 
     * @return struct E2eAddress& a reference to the address object.
     */
    struct E2eAddress& GetWorkerE2eAddr();
    
    /**
     * @brief Get a list of the worker threads
     * 
     * @return std::vector<DpdkWorkerThread> 
     */
    std::vector<DpdkWorkerThread>& GetWorkerThreads();
  
  private:

    /**  A smart pointer to the dpdk master thread object */
    std::shared_ptr<DpdkMasterThread> master_thread_;

    /** Stores all of the dpdk worker threads */
    std::vector<DpdkWorkerThread> worker_threads_;

    /** The switch end to end address in bytes with network endianness (Big endian) */
    struct E2eAddress switch_e2e_addr_be_;

    /** 
     * This worker's end to end address in bytes with network endianness (Big endian).
     * port field here is invalid since each worker thread has its own port.
     */
    struct E2eAddress worker_e2e_addr_be_;

};

} // namespace switchml
#endif // SWITCHML_DPDK_BACKEND_H_