/*
  Copyright 2021 Intel-KAUST-Microsoft

  Licensed under the Apache License, Version 2.0 (the "License");
  you may not use this file except in compliance with the License.
  You may obtain a copy of the License at

     http://www.apache.org/licenses/LICENSE-2.0

  Unless required by applicable law or agreed to in writing, software
  distributed under the License is distributed on an "AS IS" BASIS,
  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
  See the License for the specific language governing permissions and
  limitations under the License.
*/

/**
 * SwitchML Project
 * @file dpdk_master_thread_utils.inc
 * @brief Implements all functions that are needed by the dpdk_master_thread.
 * 
 * This file was created to merely reduce the size of the dpdk_master_thread files
 * and divide it into more digestable chunks. Thus only the main thread entry function is included
 * in the dpdk_master_thread and all other needed functions used by the dpdk_master_thread
 * will be included here. This file is only included once by the dpdk_master_thread.cc file
 */


#include <rte_ether.h>

#include "common.h"
#include "dpdk_backend.h"
#include "config.h"
#include "dpdk_utils.h"
#include "dpdk_worker_thread.h"

namespace switchml {

/**
 * @brief A wrapper around calling the thread object directly
 * Since DPDK does not allow that. 
 * 
 * @param arg A pointer to the worker thread to launch
 */
int LaunchDpdkWorkerThread(void* arg) {
    DpdkWorkerThread* worker_thread = static_cast<DpdkWorkerThread*>(arg);
    worker_thread->operator()();
    return 0;
}

/**
 * @brief Check whether the port/device link is up and running.
 * 
 * If its not a fatal error is thrown.
 * 
 * @param [in] port_id the port/device id to check.
 */
void CheckPortLinkStatus(uint16_t portid) {
    struct rte_eth_link link;
    VLOG(1) << "Checking link status";
    for (uint8_t count = 0; count <= 90; count++) {
        memset(&link, 0, sizeof(link));
        rte_eth_link_get_nowait(portid, &link);
        if (link.link_status) {
            VLOG(1) << "Link Up. Speed " << link.link_speed << " Mbps - " << ((link.link_duplex == ETH_LINK_FULL_DUPLEX) ? ("Full-duplex") : ("Half-duplex"));
            return;
        } else{
            rte_delay_ms(100);
        }
    }
    LOG(FATAL) << "Link Down";
}

/**
 * @brief Print information on an ethernet device.
 * 
 * @param [in] dev_info the ethernet device.
 */
void PrintDevInfo(struct rte_eth_dev_info& dev_info) {
    VLOG(1) << "Dev Info:"
        << "\n  Driver: " <<  dev_info.driver_name
        << "\n  RX buffer min size: " << dev_info.min_rx_bufsize
        << "\n  RX queues max number: " << dev_info.max_rx_queues
        << "\n  TX queues max number: " << dev_info.max_tx_queues
        << "\n  Per-port RX offload capabilities: 0x" << ToHex(dev_info.rx_offload_capa)
        << "\n  Per-port TX offload capabilities: 0x" << ToHex(dev_info.tx_offload_capa)
        << "\n  Per-queue RX offload capabilities: 0x" << ToHex(dev_info.rx_queue_offload_capa)
        << "\n  Per-queue TX offload capabilities: 0x" << ToHex(dev_info.tx_queue_offload_capa)
        << "\n  RX descriptors limits: [" << dev_info.rx_desc_lim.nb_min << "," << dev_info.rx_desc_lim.nb_max << "] aligned: "
                << dev_info.rx_desc_lim.nb_align
        << "\n  TX descriptors limits: [" << dev_info.tx_desc_lim.nb_min << "," << dev_info.tx_desc_lim.nb_max << "] aligned: "
                << dev_info.tx_desc_lim.nb_align
    ;
}

/**
 * @brief Insert flow rule to redirect ingress packets to the appropriate RX queue
 * based on the UDP port.
 * 
 * @param [in] port_id The chosen port/device id
 * @param [in] rx_q The RX queue that will receive the packets
 * @param [in] udp_dst_port The UDP port to use to forward packets to the RX queue
 * @return struct rte_flow* The flow rule that was added. This value can be ignored.
 */
struct rte_flow* InsertFlowRule(uint8_t port_id, uint16_t rx_q, uint16_t udp_dst_port) {
    // Rule attribute (ingress)
    struct rte_flow_attr attr;
    struct rte_flow_item pattern[4];
    struct rte_flow_action action[2];
    memset(&attr, 0, sizeof(struct rte_flow_attr));
    attr.ingress = 1;

    memset(pattern, 0, sizeof(pattern));
    memset(action, 0, sizeof(action));

    // Action sequence 
    struct rte_flow_action_queue queue;
    queue.index = rx_q;
    action[0].type = RTE_FLOW_ACTION_TYPE_QUEUE;
    action[0].conf = &queue;
    action[1].type = RTE_FLOW_ACTION_TYPE_END;

    // First level of the pattern (eth)
    pattern[0].type = RTE_FLOW_ITEM_TYPE_ETH;
    pattern[0].spec = 0;
    pattern[0].mask = 0;

    // Second level of the pattern (IPv4)
    struct rte_flow_item_ipv4 ip_spec;
    struct rte_flow_item_ipv4 ip_mask;
    memset(&ip_spec, 0, sizeof(struct rte_flow_item_ipv4));
    memset(&ip_mask, 0, sizeof(struct rte_flow_item_ipv4));
    ip_spec.hdr.next_proto_id = 17;
    ip_mask.hdr.next_proto_id = 0xFF;
    pattern[1].type = RTE_FLOW_ITEM_TYPE_IPV4;
    pattern[1].spec = &ip_spec;
    pattern[1].mask = &ip_mask;

    // Third level of the pattern (UDP)
    struct rte_flow_item_udp udp_spec;
    struct rte_flow_item_udp udp_mask;
    memset(&udp_spec, 0, sizeof(struct rte_flow_item_udp));
    memset(&udp_mask, 0, sizeof(struct rte_flow_item_udp));
    udp_spec.hdr.dst_port = rte_cpu_to_be_16(udp_dst_port);
    udp_mask.hdr.dst_port = 0xFFFF;
    pattern[2].type = RTE_FLOW_ITEM_TYPE_UDP;
    pattern[2].spec = &udp_spec;
    pattern[2].mask = &udp_mask;

    // Final level
    pattern[3].type = RTE_FLOW_ITEM_TYPE_END;

    struct rte_flow_error error;
    LOG_IF(FATAL, rte_flow_validate(port_id, &attr, pattern, action, &error) != 0)
        << "Flow rule can't be added: " << error.type << (error.message ? error.message : "(no stated reason)");

    struct rte_flow* flow = NULL;
    flow = rte_flow_create(port_id, &attr, pattern, action, &error);

    return flow;
}


/**
 * @brief Configure and start the chosen port/device
 * 
 * The function sets up the following for each worker thread
 * - Configures the port/device
 * - Prints the device information using PrintDevInfo()
 * - Allocates an RX mempool for each worker thread 
 *   (Needed now to initialize RX queues, the TX mempool can be allocated later by the worker threads)
 * - Configures and initializes RX and TX queues
 * - Starts the port/device
 * - Checks that the link is up using CheckPortLinkStatus()
 * - Adds flow rules using InsertFlowRule() to redirect ingress packets to appropriate worker
 *   thread queues based on the UDP port.
 * 
 * This function is called by the DpdkMasterThread during initialization after
 * the EAL is initialized.
 * 
 * @param [in] num_worker_threads The number of worker threads so that the function the appropriate
 * data structures for each worker thread.
 */
void InitPort(DpdkBackendConfig config, uint16_t num_worker_threads) {
    int ret;
    uint16_t num_queues = num_worker_threads;

    // Check number of ports
    uint16_t nb_ports = rte_eth_dev_count_avail();
    LOG_IF(FATAL, nb_ports == 0) << "No Ethernet ports";

    // Is the port id in the configuration available?
    uint16_t port_id = config.port_id;
    uint16_t tmp_port_id;
    bool found_port_id = false;
    RTE_ETH_FOREACH_DEV(tmp_port_id)
    {
        if (port_id == tmp_port_id)
        {
            found_port_id = true;
            break;
        }
    }
    LOG_IF(FATAL, !found_port_id) << "Port ID " << port_id << " is not available. The number of enabled ports is " << nb_ports;
    VLOG(1) << "Initializing port " << port_id << "...";

    // Get port info
    struct rte_eth_dev_info dev_info;
    rte_eth_dev_info_get(port_id, &dev_info);

    PrintDevInfo(dev_info);

    // Port configuration
    struct rte_eth_conf port_conf = { };
    struct rte_eth_rxmode rxm = { };
    struct rte_eth_txmode txm = { };
    struct rte_eth_rxconf rx_conf = { };
    struct rte_eth_txconf tx_conf = { };

    rxm.split_hdr_size = 0;
    //rxm.ignore_offload_bitfield = 1;

    if (dev_info.rx_offload_capa & DEV_RX_OFFLOAD_IPV4_CKSUM) {
        rxm.offloads |= DEV_RX_OFFLOAD_IPV4_CKSUM;
        VLOG(1) << "RX IPv4 checksum offload enabled";
    }

    if (dev_info.rx_offload_capa & DEV_RX_OFFLOAD_UDP_CKSUM) {
        rxm.offloads |= DEV_RX_OFFLOAD_UDP_CKSUM;
        VLOG(1) << "RX UDP checksum offload enabled";
    }

    /* Default in DPDK 18.11
    if (dev_info.rx_offload_capa & DEV_RX_OFFLOAD_CRC_STRIP) {
        rxm.offloads |= DEV_RX_OFFLOAD_CRC_STRIP;
        VLOG(1) << "RX CRC stripped by the hw";
    }*/

    // TX Configuration
    txm.mq_mode = ETH_MQ_TX_NONE;

    if (dev_info.tx_offload_capa & DEV_TX_OFFLOAD_IPV4_CKSUM) {
        txm.offloads |= DEV_TX_OFFLOAD_IPV4_CKSUM;
        VLOG(1) << "TX IPv4 checksum offload enabled";
    }

    if (dev_info.tx_offload_capa & DEV_TX_OFFLOAD_UDP_CKSUM) {
        txm.offloads |= DEV_TX_OFFLOAD_UDP_CKSUM;
        VLOG(1) << "TX UDP checksum offload enabled";
    }

    if (dev_info.tx_offload_capa & DEV_TX_OFFLOAD_MBUF_FAST_FREE) {
        txm.offloads |= DEV_TX_OFFLOAD_MBUF_FAST_FREE;
        VLOG(1) << "Fast release of mbufs enabled";
    }

    port_conf.rxmode = rxm;
    port_conf.txmode = txm;
    //port_conf.link_speeds = ETH_LINK_SPEED_AUTONEG;
    //port_conf.lpbk_mode = 0; // Loopback operation mode disabled
    //port_conf.dcb_capability_en = 0; // DCB disabled

    // Flow director
    port_conf.rx_adv_conf.rss_conf.rss_key = NULL;
    port_conf.rx_adv_conf.rss_conf.rss_hf = 0;

    port_conf.fdir_conf.mode = RTE_FDIR_MODE_PERFECT;
    port_conf.fdir_conf.pballoc = RTE_FDIR_PBALLOC_64K;
    port_conf.fdir_conf.status = RTE_FDIR_NO_REPORT_STATUS;
    port_conf.fdir_conf.drop_queue = 127;

    //memset(&port_conf.fdir_conf.mask, 0x00, sizeof(struct rte_eth_fdir_masks));
    //port_conf.fdir_conf.mask.dst_port_mask = 0xFFFF;

    struct rte_flow_error error;

    LOG_IF(ERROR, rte_flow_isolate(port_id, 1, &error) < 0)
        << "Flow isolated mode failed: " << error.type << " " << (error.message ? error.message : "(no stated reason)");

    ret = rte_eth_dev_configure(port_id, num_queues, num_queues, &port_conf);
    LOG_IF(FATAL, ret < 0) << "Cannot configure port: " << rte_strerror(ret);

    uint16_t port_rx_ring_size;
    uint16_t port_tx_ring_size;

    // TODO: Set this to max outstanding pkts per worker thread. Maybe 2 times that.
    port_rx_ring_size = dev_info.rx_desc_lim.nb_max < 1024 ? dev_info.rx_desc_lim.nb_max : 1024;
    port_tx_ring_size = dev_info.tx_desc_lim.nb_max < 1024 ? dev_info.tx_desc_lim.nb_max : 1024;

    // Check that numbers of Rx and Tx descriptors satisfy descriptors
    // limits from the ethernet device information, otherwise adjust
    // them to boundaries.
    ret = rte_eth_dev_adjust_nb_rx_tx_desc(port_id, &port_rx_ring_size, &port_tx_ring_size);
    LOG_IF(FATAL, ret < 0) << "Cannot adjust number of descriptors: " << rte_strerror(ret);

    //Get the port address
    struct rte_ether_addr port_eth_addr;
    rte_eth_macaddr_get(port_id, &port_eth_addr);

    // init RX queue
    rx_conf = dev_info.default_rxconf;
    rx_conf.offloads = port_conf.rxmode.offloads;
    //rx_conf.rx_thresh.pthresh = 8;
    //rx_conf.rx_thresh.hthresh = 8;
    //rx_conf.rx_thresh.wthresh = 4;
    //rx_conf.rx_free_thresh = 64;
    //rx_conf.rx_drop_en = 0;

    // init TX queue on each port
    tx_conf = dev_info.default_txconf;
    //tx_conf.txq_flags = ETH_TXQ_FLAGS_IGNORE;
    tx_conf.offloads = port_conf.txmode.offloads;
    //tx_conf.tx_thresh.pthresh = 36;
    //tx_conf.tx_thresh.hthresh = 0;
    //tx_conf.tx_thresh.wthresh = 0;
    //tx_conf.tx_free_thresh = 0;
    //tx_conf.tx_rs_thresh = 0;

    for (uint32_t i=0; i<num_queues; i++){
        VLOG(1) << "Initializing rx and tx queues for worker thread '" << i << "'.";
        // Init the rx mempool
        std::string rx_mempool_name = "wt" + std::to_string(i) + "_rx";
        struct rte_mempool *rx_mempool;
        rx_mempool = rte_pktmbuf_pool_create(rx_mempool_name.c_str(), config.pool_size, config.pool_cache_size, 0, RTE_MBUF_DEFAULT_BUF_SIZE, rte_socket_id());
        LOG_IF(FATAL, rx_mempool == NULL) << "Cannot init mbuf pool: " << rte_strerror(rte_errno);

        // Init queues
        ret = rte_eth_rx_queue_setup(port_id, i, port_rx_ring_size, rte_eth_dev_socket_id(port_id), &rx_conf, rx_mempool);
        LOG_IF(FATAL, ret < 0) << "RX queue setup error: " << rte_strerror(ret);

        ret = rte_eth_tx_queue_setup(port_id, i, port_tx_ring_size, rte_eth_dev_socket_id(port_id), &tx_conf);
        LOG_IF(FATAL, ret < 0) << "TX queue setup error: " << rte_strerror(ret);
    }

    // Start device
    ret = rte_eth_dev_start(port_id);
    LOG_IF(FATAL, ret < 0) << "Error starting the port: " << rte_strerror(ret);

    // Enable promiscuous mode
    //rte_eth_promiscuous_enable(dpdk_par.portid);

    VLOG(1) << "Initialization ended. Port " << port_id << " address: " << Mac2Str(port_eth_addr);


    CheckPortLinkStatus(port_id);

    // Add FDIR filters
    for (uint16_t i = 0; i < num_queues; i++){
        InsertFlowRule(port_id, i, config.worker_port+i);
    }
}

} // namespace switchml