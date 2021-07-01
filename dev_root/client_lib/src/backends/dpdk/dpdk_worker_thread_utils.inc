/**
 * SwitchML Project
 * @file dpdk_worker_thread_utils.inc
 * @brief Implements all functions that are needed by the dpdk_worker_thread.
 * 
 * This file was created to merely reduce the size of the dpdk_worker_thread files
 * and divide it into more digestable chunks. Thus only the main thread entry function is included
 * in the dpdk_worker_thread and all other needed functions used by the dpdk_worker_thread
 * will be included here. This file is only included once by the dpdk_worker_thread.cc file
 */

#include <dpdk_backend.h>
#include <rte_ether.h>

#include "common.h"
#include "dpdk_utils.h"
#include "prepostprocessor.h"
#include "dpdk_worker_thread.h"

namespace switchml {

/**
 * @brief Compute the switch's pool index from a packet id
 */
__rte_always_inline
uint16_t PktId2PoolIndex(uint64_t pkt_id, uint32_t switch_pool_index_start,
                         uint32_t switch_pool_index_shift, uint32_t max_outstanding_pkts) {
    uint32_t i = (pkt_id + switch_pool_index_shift) % (2 * max_outstanding_pkts);
    if (i < max_outstanding_pkts) {
        // Set MSB to 0 (We use the 1st switch pool)
        return (switch_pool_index_start + i);
    } else {
        // Set MSB to 1 (We use the 2nd switch pool)
        return ((switch_pool_index_start + (i - max_outstanding_pkts)) | 0x8000);
    }
}

/**
 * @brief A function to fill an empty mbuf with all required headers and payload
 * 
 * @param mbuf A pointer to an allocated mbuf.
 * @param job_id The id of the job corresponding to this packet.
 * @param pkt_id The id of the packet to send within this job slice.
 * @param switch_pool_index The switch pool/slot index to use.
 * @param packet_numel The number of elements in a packet (From the configuration)
 * @param switch_e2e_addr_be The switch's end to end address in big endian
 * @param worker_thread_e2e_addr_be The worker thread's end to end address in big endian
 * @param ppp a pointer to the prepostprocesser used by the worker thread
 */
__rte_always_inline 
void BuildPacket(rte_mbuf* mbuf, JobId job_id, uint32_t pkt_id,
                 uint16_t switch_pool_index, uint64_t packet_numel,
                 DpdkBackend::E2eAddress switch_e2e_addr_be,
                 DpdkBackend::E2eAddress worker_thread_e2e_addr_be,
                 std::shared_ptr<PrePostProcessor> ppp) {

    mbuf->data_len = sizeof(struct rte_ether_hdr) + sizeof(struct rte_ipv4_hdr) + sizeof(struct rte_udp_hdr)
        + sizeof(struct DpdkBackend::DpdkPacketHdr) + packet_numel * sizeof(DpdkBackend::DpdkPacketElement) + 2; // + 2 bytes space for prepostprocessor extra info
    mbuf->pkt_len = mbuf->data_len;

    // 0. Mbuf metadata (For the checksum offload)
    mbuf->l2_len = sizeof(struct rte_ether_hdr);
    mbuf->l3_len = sizeof(struct rte_ipv4_hdr);
    mbuf->ol_flags |= PKT_TX_IP_CKSUM | PKT_TX_IPV4 | PKT_TX_UDP_CKSUM; 
    rte_prefetch0 (rte_pktmbuf_mtod(mbuf, void *));

    // 1. Ethernet header
    struct rte_ether_hdr *ether = rte_pktmbuf_mtod(mbuf, struct rte_ether_hdr *);
    // Set MAC addresses
    memcpy(ether->s_addr.addr_bytes, &worker_thread_e2e_addr_be.mac, 6);
    memcpy(ether->d_addr.addr_bytes, &switch_e2e_addr_be.mac, 6);

    // Set ethertype
    ether->ether_type = rte_cpu_to_be_16(RTE_ETHER_TYPE_IPV4);

    // 2. IP header
    struct rte_ipv4_hdr *ip = reinterpret_cast<struct rte_ipv4_hdr*>(ether + 1);
    ip->version_ihl = 0x45;
    ip->total_length = rte_cpu_to_be_16(mbuf->data_len - sizeof(struct rte_ether_hdr));
    ip->time_to_live = 128;
    ip->next_proto_id = IPPROTO_UDP;
    ip->hdr_checksum = 0;
    ip->src_addr = worker_thread_e2e_addr_be.ip;
    ip->dst_addr = switch_e2e_addr_be.ip;

    // 3. UDP header
    struct rte_udp_hdr *udp = reinterpret_cast<struct rte_udp_hdr*>(ip + 1);
    udp->src_port = worker_thread_e2e_addr_be.port; 
    udp->dst_port = switch_e2e_addr_be.port;
    udp->dgram_len = rte_cpu_to_be_16(mbuf->data_len - sizeof(struct rte_ether_hdr) - sizeof(struct rte_ipv4_hdr));
    udp->dgram_cksum = rte_ipv4_phdr_cksum(ip, mbuf->ol_flags);

    // 4. Switchml header
    // Categorize the length of the packet
    // TODO: Remove this as this information is already sent to the controller at
    // grpc setup.
    uint8_t pkt_len_enum;
    if (packet_numel < 64) {
        pkt_len_enum = 0;
    } else if (packet_numel < 128) {
        pkt_len_enum = 1;
    } else if (packet_numel < 256) {
        pkt_len_enum = 2;
    } else {
        pkt_len_enum = 3;
    }
    struct DpdkBackend::DpdkPacketHdr* switchml_pkt_hdr = reinterpret_cast<struct DpdkBackend::DpdkPacketHdr*>(udp + 1);
    // We only need to change to network byte order if the field will be used by the switch.
    // And in our case, only switch_pool_index and job_type_size will be used by the switch.
    switchml_pkt_hdr->pkt_id = pkt_id;
    switchml_pkt_hdr->switch_pool_index = rte_cpu_to_be_16(switch_pool_index);
    switchml_pkt_hdr->job_type_size = (1 << 4) + pkt_len_enum; // bit shift for host to network byte order
    switchml_pkt_hdr->short_job_id = job_id; // Select the 8 LSBs of the job id

    // Preprocess the packet (Copying/quantizing is done here)
    DpdkBackend::DpdkPacketElement* entries_ptr = reinterpret_cast<DpdkBackend::DpdkPacketElement*>(switchml_pkt_hdr+1);
    void* extra_info_ptr = entries_ptr + packet_numel;
    ppp->PreprocessSingle(pkt_id, entries_ptr, extra_info_ptr);
}

/**
 * @brief Reuse a received mbuf to be sent once again with new data.
 * 
 * @param mbuf A pointer to the received mbuf
 * @param pkt_id The id of the packet to send within this job slice.
 * @param switch_pool_index The updated/new switch pool index
 * @param switch_e2e_addr_be The switch's end to end address in big endian
 * @param worker_thread_e2e_addr_be The worker thread's end to end address in big endian
 * @param ppp a pointer to the prepost processer used by the worker thread
 */
__rte_always_inline
void ReusePacket(rte_mbuf* mbuf, uint32_t pkt_id, uint64_t packet_numel,
                 uint16_t switch_pool_index, DpdkBackend::E2eAddress switch_e2e_addr_be,
                 DpdkBackend::E2eAddress worker_thread_e2e_addr_be, std::shared_ptr<PrePostProcessor> ppp) {
    // 1. Set MACs
    struct rte_ether_hdr* ether = rte_pktmbuf_mtod(mbuf, struct rte_ether_hdr*);
    memcpy(ether->s_addr.addr_bytes, &worker_thread_e2e_addr_be.mac, 6);
    memcpy(ether->d_addr.addr_bytes, &switch_e2e_addr_be.mac, 6);

    // 2. Set IPs
    struct rte_ipv4_hdr * const ip = reinterpret_cast<struct rte_ipv4_hdr*>(ether + 1);
    ip->hdr_checksum = 0;
    ip->src_addr = worker_thread_e2e_addr_be.ip;
    ip->dst_addr = switch_e2e_addr_be.ip;

    // 3. Set UDP
    struct rte_udp_hdr * const udp = reinterpret_cast<struct rte_udp_hdr*>(ip + 1);
    udp->src_port = worker_thread_e2e_addr_be.port;
    udp->dst_port = switch_e2e_addr_be.port;
    udp->dgram_cksum = rte_ipv4_phdr_cksum(ip, mbuf->ol_flags);

    // 4. Switchml header
    struct DpdkBackend::DpdkPacketHdr* switchml_pkt_hdr = reinterpret_cast<struct DpdkBackend::DpdkPacketHdr*>(udp + 1);
    switchml_pkt_hdr->pkt_id = pkt_id;
    // Swap pool index msb to alternate between the pools
    switchml_pkt_hdr->switch_pool_index = rte_cpu_to_be_16(switch_pool_index);

    // Preprocess the packet (Copying/quantizing is done here)
    DpdkBackend::DpdkPacketElement* entries_ptr = reinterpret_cast<DpdkBackend::DpdkPacketElement*>(switchml_pkt_hdr+1);
    void* extra_info_ptr = entries_ptr + packet_numel;
    ppp->PreprocessSingle(pkt_id, entries_ptr, extra_info_ptr);
}


/**
 * @brief The callback to call when some packets fail to be sent.
 * 
 * This cannot be bound to any class so we include it here.
 * It simply loops untill all failed packets have been successfully sent.
 * 
 * @param pkts the packets that failed to be sent.
 * @param unsent the number of packets that failed to be sent.
 * @param wt_ptr a pointer to the worker thread.
 */
void TxBufferCallback(struct rte_mbuf **pkts, uint16_t unsent, void *wt_ptr) {
    DpdkWorkerThread* dwt = static_cast<DpdkWorkerThread*>(wt_ptr);
    DVLOG(3) << "Worker thread '" << dwt->tid_ << "' TX buffer error: unsent " << unsent;
    unsigned nb_tx = 0, sent = 0;

    do {
        nb_tx = rte_eth_tx_burst(dwt->config_.backend_.dpdk.port_id, dwt->tid_, &pkts[sent], unsent - sent);
        sent += nb_tx;
    } while (sent < unsent);
}


#ifdef TIMEOUTS

// SUGGESTION: try to avoid this struct as its not very clean
/**
 * @brief Just a simple struct to pass needed information to 
 * The ResendPacketCallback function
 */
struct ResendPacketCallbackArgs {
    JobId job_id;
    uint32_t pkt_id;
    uint16_t switch_pool_index;
    rte_mempool* tx_mempool;
    DpdkWorkerThread* dwt;
    std::shared_ptr<PrePostProcessor> ppp;
};

/**
 * @brief A callback to rebuild and resend a packet when a timeout occurs.
 * 
 * @param timer the timer that expired
 * @param arg A pointer to ResendPacketCallbackArgs
 */
void ResendPacketCallback(struct rte_timer *timer, void *arg) {
    // SUGGESTION: Reuse mbufs instead of allocating new ones.

    struct ResendPacketCallbackArgs* args = static_cast<struct ResendPacketCallbackArgs*>(arg);
    static thread_local uint64_t timeouts_counter = 0;
    static thread_local uint64_t timeout_threshold = args->dwt->config_.general_.timeout_threshold;
    timeouts_counter++;
    
    DVLOG(3) << "Timeout occured. Resending packet short_job_id=" << (uint8_t) args->job_id << " pkt_id="
                    << args->pkt_id;

    // If timeouts_counter exceeded the threshold then update the timer cycles
    if (timeouts_counter >= timeout_threshold) {
        DVLOG(3) << "Timeouts counter reached " << timeouts_counter << " resetting it and increasing the timeout threshold from " 
            << timeout_threshold << " to " << timeout_threshold + args->dwt->config_.general_.timeout_threshold_increment
            << " and timer cycles from " << args->dwt->timer_cycles_  << " to " << args->dwt->timer_cycles_ * 2;
        timeouts_counter = 0;
        timeout_threshold += args->dwt->config_.general_.timeout_threshold_increment;
        args->dwt->timer_cycles_ *= 2;
    }

    // Allocate packet
    struct rte_mbuf* mbufs[1];
    mbufs[0] = rte_pktmbuf_alloc(args->tx_mempool);
    LOG_IF(FATAL, unlikely(mbufs == NULL)) << "Cannot allocate packet in the ResendPacketCallback";

    // Build packet
    BuildPacket(mbufs[0], args->job_id, args->pkt_id, args->switch_pool_index,
        args->dwt->config_.general_.packet_numel,
        args->dwt->backend_.GetSwitchE2eAddr(),
        args->dwt->worker_thread_e2e_addr_be_,
        args->ppp);
    
    // Loop until its successfully sent
    while (rte_eth_tx_burst(args->dwt->config_.backend_.dpdk.port_id, args->dwt->tid_, mbufs, 1) == 0);

    rte_timer_reset_sync(timer, args->dwt->timer_cycles_, PERIODICAL, args->dwt->lcore_id_, ResendPacketCallback, arg);

    Context::GetInstance().GetStats().AddTimeouts(args->dwt->tid_, 1);
    Context::GetInstance().GetStats().AddTotalPktsSent(args->dwt->tid_, 1);
}
#endif

} // namespace switchml