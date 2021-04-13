/**
 * SwitchML Project
 * @file config.h
 * @brief Declares the Config class. Here you find all configurable options for SwitchML.
 */

#ifndef SWITCHML_CONFIG_H_
#define SWITCHML_CONFIG_H_

#include <string>

#include "common.h"

namespace switchml {

/**
 * @brief Struct that groups general configuration options that must always be configured.
 */
struct GeneralConfig {
    /** A unique identifier for a worker node. Like MPI ranks. */
    uint16_t rank;

    /** The number of worker nodes in the system */
    uint16_t num_workers;

    /** The number of worker threads to launch for each node */
    uint16_t num_worker_threads;

    /**
     * The maximum number of pending packets for this **worker** (Not worker thread).
     * 
     * This number is divided between worker threads.
     * This means that each worker thread will first send its initial
     * burst up to this number divided by num_worker_threads. Then sends new packets only after packets are received
     * doing this until all packets have been sent.
     * 
     * If you have this set to 256 and num_worker_threads set to 8 then each worker thread will send up to 32 packets.
     */
    uint32_t max_outstanding_packets;
    
    /**
     * The number of elements in a packet
     */
    uint64_t packet_numel;

    /**
     * Which backend should the SwitchML client use?. Choose from ['dummy', 'dpdk', 'rdma'].
     * Make sure that the backend you choose has been compiled.
     */ 
    std::string backend;

    /** Which scheduler should we use to dispatch jobs to worker threads?. Choose from ['fifo']. */
    std::string scheduler;

    /** Which prepostprocessor should we use to load and unload the data into and from the network. Choose from ['bypass', 'cpu_exponent_quantizer'] */
    std::string prepostprocessor;

    /** 
     * If set to true then all jobs will be instantly completed regardless of the job type.
     * This is used for debugging to disable all backend communication.
     * The backend is still used to for setup and cleanup.
    */
    bool instant_job_completion;
};


#ifdef DPDK
/**
 * @brief Configuration options specific to using the DPDK backend.
 */
struct DpdkBackendConfig {
    /** The worker's udp port. No restrictions here just choose a port that is unused by any application */
    uint16_t worker_port;

    /**
     * Worker IP in the dotted decimal notation
     * Choose the IP address for this worker and make sure its the one that
     * corresponds to the correct network interface that you want to use for communication. 
     */
    std::string worker_ip_str;

    /**
     * Switch UDP port
     * The switch uses this number to identify whether a packet belongs
     * to switchml or if its normal traffic. So in order to ensure that the switch
     * knows that this is switchml traffic set this port to something between
     * 45056 and 49151.
     */
    uint16_t switch_port;

    /**
     * Switch IP address in the dotted decimal notation
     * This must be on the same subnet as the workers and should match
     * the ip address that was passed to the switch python controller via 
     * the --switch_ip command line argument. Other than these restrictions
     * you are free to choose the ip that you want as long as its unique in the
     * local network.
     */
    std::string switch_ip_str;

    /**
     * Switch mac address
     * This should match the mac address that was passed to the switch python
     * controller via the --switch_mac command line argument. Other than 
     * this restriction you are free to choose the mac that you want as long
     * as its unique in the local network.
     */
    std::string switch_mac_str;

    /**
     * The DPDK core configuration
     * As you know, each worker can have multiple threads/cores. 
     * Here we specify the specific cores that we want to use 
     * For Ex. cores = 10-13 will use 4 cores 10 through 13.
     * cores = 10 will only use the core numbered 10).
     * For best performance, you should use cores that are on the same NUMA node as the NIC.
     * To do this do the following:
     * - Run `sudo lshw -class network -businfo` and take note of the PCIe identifier of the NIC (Ex. 0000:81:00.0).
     * - Run `lspci -s 0000:81:00.0 -vv | grep NUMA` to find out out the NUMA node at which this NIC resides.
     * - Run `lscpu | grep NUMA` to know the core numbers that also reside on the same NUMA node.
     * 
     * **make sure the number of cores you choose matches the number of worker threads in the general config**
     */
    std::string cores_str;

    /**
     * These are extra options that will be passed to DPDK EAL. What we should include here 
     * is the PCIe identifier of the NIC using the -w option. (Ex. '-w 0000:81:00.0' )
     * to make sure that DPDK is using the right NIC.
     * Otherwise you should find out the port id of the nic that you want.
     */
    std::string extra_eal_options;

    /**
     * Each NIC has an associated port id. Basically this is the index into the list of available
     * NICs. If you've white listed the NIC that you want in extra_eal_options then you can always leave this
     * as 0 since your chosen NIC will be the only one in the list.
     */
    uint16_t port_id;

    /** 
     * The size of the memory pool size for each worker thread. 
     * A memory pool is a chunk of memory from the huge pages which we use to allocate mbufs.
     * Each worker thread will have its own memory pool for receiving mbufs (packets) and another for
     * creating mbufs to be sent. Thus the total size of all memory pools can be calculated as 
     * pool_size*num_worker_threads*2. So just make sure you don't try to overallocate space that
     * you don't have.
    */
    uint32_t pool_size;

    /** 
     * Each memory pool as described in pool_size has a dedicated software cache. How big do we want this to be?  
     * This value has strict restrictions from DPDK. If you don't know what you're doing you can leave it as it is.
     */
    uint32_t pool_cache_size;
    
    /** What's the maximum number of packets that we retrieve from the NIC at a time. */
    uint32_t burst_rx;

    /** What's the maximum number of packets that we push onto the NIC at a time. */
    uint32_t burst_tx;

    /** Using what period in microseconds should we flush the transmit buffer? */
    uint32_t bulk_drain_tx_us;

#ifdef TIMEOUTS
    /**
     * How much time in ms should we wait before we consider that a packet is lost.
     * 
     * Each worker thread creates a copy of this value at the start of working on a job slice.
     * From that point the timeout value can be increased if the number of timeouts exceeds a threshold
     * as a backoff mechanism.
     */
    double timeout;

    /** How many timeouts should occur before we double the timeout time? */
    uint64_t timeout_threshold;
    
    /** 
     * By how much should we increment the threshold each time its exceeded.
     * (Setting the bar higher to avoid doubling the timouet value too much) 
     */
    uint64_t timeout_threshold_increment;
#endif

};
#endif

#ifdef RDMA
/**
 * @brief Configuration options specific to using the RDMA backend.
 */
struct RdmaBackendConfig {

};
#endif

/**
 * @brief Configuration options specific to using the Dummy backend.
 */
struct DummyBackendConfig {
    /**
     * The bandwidth (in Mbps) that will be used to calculate sleeping durations for communication
     * Set it to 0 to disable sleeping.
     */
    float bandwidth;

    /**
     * Should the dummy backend actually compute what the tensor values would be if it had done
     * Real aggregation? that is should it multiply each tensor element by the number of workers?
     * With a real backend this would be done on the switch not slowly on our CPU.
     */
    bool process_packets;
};

/**
 * @brief The struct that groups all backend related options.
 */
struct BackendConfig {
#ifdef DPDK
    struct DpdkBackendConfig dpdk;
#endif
#ifdef RDMA
    struct RdmaBackendConfig rdma;
#endif
    struct DummyBackendConfig dummy;
};

/**
 * @brief A class that is responsible for parsing and representing all configurable options for SwitchML.
 */
class Config {
  public:
    Config() = default;
    ~Config() = default;

    Config(Config const&) = default;
    void operator=(Config const&);

    Config(Config&&) = default;
    Config& operator=(Config&&) = default;

    /**
     * @brief Read and parse the configuration file.
     *
     * @param [in] path the path of the configuration file or nullptr.
     * If the path was ommited then the function looks for the file in
     * the following default paths in order:
     *  1- /etc/switchml.cfg
     *  2- ./switchml.cfg
     *  3- ./switchml-<hostname>.cfg  (Ex. ./switchml-node12.config)
     * @return loading was successfull
     * @return loading failed.
     */
    bool LoadFromFile(std::string path = "");

    /**
     * @brief Make sure configuration values are valid.
     * 
     * If a misconfiguration is fatal then it shuts the program down.
     */
    void Validate();

    /**
     * @brief Print all configuration options.
     */
    void PrintConfig();

    /** General configuration options */
    struct GeneralConfig general_;

    /** Backend specific configuration options */
    struct BackendConfig backend_;
};

} // namespace switchml

#endif // SWITCHML_CONFIG_H_