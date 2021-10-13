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
 * @file dpdk_worker_thread.cc
 * @brief Implements the DpdkWorkerThread class.
 */

#include "dpdk_worker_thread.h"

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <rte_malloc.h>
#include <rte_bitmap.h>
#include <rte_mbuf.h>

#include <vector>
#include <algorithm>

#include "common_cc.h"

#include "dpdk_worker_thread_utils.inc"

namespace switchml {

WorkerTid DpdkWorkerThread::next_tid_ = 0;

DpdkWorkerThread::DpdkWorkerThread(Context& context, DpdkBackend& backend, Config& config) :
    tid_(DpdkWorkerThread::next_tid_++),
    context_(context),
    backend_(backend),
    config_(config),
    worker_thread_e2e_addr_be_(backend.GetWorkerE2eAddr()),
    lcore_id_(0), // will be correctly set when the thread starts
    ppp_()
#ifdef TIMEOUTS
    ,timer_cycles_(0)
#endif
{
    // Initializing objects by the thread that will use them improves performance slightly.
    // Thus we initialize the PPP in operator()
}

DpdkWorkerThread::~DpdkWorkerThread(){
    // Do nothing
}

void DpdkWorkerThread::operator()() {
    int ret;

    // Each worker thread has its own udp source ports
    this->worker_thread_e2e_addr_be_.port = rte_cpu_to_be_16(
                                                rte_be_to_cpu_16(this->worker_thread_e2e_addr_be_.port)
                                                + this->tid_
                                            );
    this->lcore_id_ = rte_lcore_id();

    VLOG(0) << "Worker thread '" << this->tid_ << "' starting on core '" << this->lcore_id_ << "'";

    // Shortcuts
    Context& ctx = this->context_;
    DpdkBackend& bk = this->backend_;
    const GeneralConfig& genconf = this->config_.general_;
    const DpdkBackendConfig& dpdkconf = this->config_.backend_.dpdk;

    // The maximum number of outstanding packets for this worker.
    const uint64_t max_outstanding_pkts = genconf.max_outstanding_packets/genconf.num_worker_threads;

    this->ppp_ = PrePostProcessor::CreateInstance(this->config_,
                 this->tid_, genconf.packet_numel * DPDK_SWITCH_ELEMENT_SIZE, max_outstanding_pkts);

    // Explaining switch pool / slot:
    // The switch can be thought of as a pool of slots (array of slots). Slots holds/adds the contents of
    // a single packet. So the pool size must be = max_outstanding_pkts. Now to be able to do retransmissions
    // from the switch side we need to keep a shadow copy of each slot so now our switch's pool has doubled
    // in size or you can think of it as we now have two pools. Furthermore, each worker thread will only work on a
    // subset of the pool defined as [switch_pool_index_start, switch_pool_index_start + max outstanding packets for the worker thread * 2]
    uint32_t switch_pool_size = max_outstanding_pkts * 2;
    uint32_t switch_pool_index_start = switch_pool_size * this->tid_;

    // For the switch to function properly we must always use an incremental pool index.
    // we cannot suddenly stop and start from pool index 0. This is due to some implementation detail in 
    // our P4 program that is out of our scope here. So this shift keeps track of where the last job finished so that
    // the new job can pick up from the next pool index.
    uint32_t switch_pool_index_shift = 0;

    // Create a mempool from which we will allocate the mbufs to transmit.  (Think of an mbuf as DPDK's representation of a packet)
    std::string tx_mempool_name = "wt" + std::to_string(this->tid_) + "_tx";
    struct rte_mempool* tx_mempool = rte_pktmbuf_pool_create(tx_mempool_name.c_str(), dpdkconf.pool_size, dpdkconf.pool_cache_size,
        0, RTE_MBUF_DEFAULT_BUF_SIZE, rte_socket_id());
    LOG_IF(FATAL, tx_mempool == NULL) << "Worker thread '" << this->tid_ << "' Cannot init mbuf pool: " << rte_strerror(rte_errno);

    // Allocate an array of mbuf pointers to store pointers to the mbufs we will transmit
    // We use rte_malloc_socket to make sure that the memory we are using is closest to the core we are using.
    struct rte_mbuf **pkts_tx_burst = (rte_mbuf **) rte_malloc_socket(NULL, max_outstanding_pkts * sizeof(struct rte_mbuf*), RTE_CACHE_LINE_SIZE, rte_socket_id());
    LOG_IF(FATAL, pkts_tx_burst == NULL) << "Worker thread '" << this->tid_ << "' Cannot allocate pkts tx burst";

    // Allocate the actual mbufs that we will transmit.
    ret = rte_pktmbuf_alloc_bulk(tx_mempool, pkts_tx_burst, max_outstanding_pkts);
    LOG_IF(FATAL, ret < 0) << "Worker thread '" << this->tid_ << "' Cannot allocate mbuf tx burst";

    // The tx buffer is used to buffer packets and group them up before sending them
    // This improves performance as opposed to issuing a single send using rte_eth_tx_burst to send
    // packets one by one. So we use rte_eth_tx_burst for the first batch because they are already 
    // grouped up but then use rte_eth_tx_buffer to send the packets afterwards.
    struct rte_eth_dev_tx_buffer* tx_buffer = (rte_eth_dev_tx_buffer*) rte_zmalloc_socket("tx_buffer",
        RTE_ETH_TX_BUFFER_SIZE(dpdkconf.burst_tx), RTE_CACHE_LINE_SIZE, rte_socket_id());
    LOG_IF(FATAL, tx_buffer == NULL) << "Worker thread '" << this->tid_ << "' Cannot allocate TX buffer";
    rte_eth_tx_buffer_init(tx_buffer, dpdkconf.burst_tx);
    ret = rte_eth_tx_buffer_set_err_callback(tx_buffer, TxBufferCallback, this);
    LOG_IF(FATAL, ret < 0) << "Worker thread '" << this->tid_ << "' Cannot set callback for tx buffer";

    // Time Stamp Counter TSC. (Number of CPU cycles since reset)
    // These are used to force flush the tx buffer. Because the tx buffer only sends the packets when its
    // full so we need a mechanism to flush it in cases where it does not get full for a while.
    uint64_t prev_tsc = 0;
    uint64_t cur_tsc = 0;
    const uint64_t drain_tsc = (rte_get_tsc_hz() + US_PER_S - 1) / US_PER_S * dpdkconf.bulk_drain_tx_us;

    // Allocate an array of mbuf pointers to store pointers to the mbufs we will receive
    // Remember that the mempool for the mbufs that we will receive was already created by the master
    // thread in the InitPort function so we only need to allocate space for the pointers. The actual mbufs
    // will be created automatically upon receiving packets.
    struct rte_mbuf **pkts_rx_burst = (rte_mbuf **) rte_malloc_socket(NULL, max_outstanding_pkts * sizeof(struct rte_mbuf*), RTE_CACHE_LINE_SIZE, rte_socket_id());
    LOG_IF(FATAL, pkts_rx_burst == NULL) << "Worker thread '" << this->tid_ << "' Cannot allocate pkts rx burst";

#ifdef TIMEOUTS
    // Initialize timer library
    rte_timer_subsystem_init();

    const uint64_t initial_timer_cycles = (rte_get_timer_hz() / 1000) * genconf.timeout;
    // The actual timer structs
    struct rte_timer timers[max_outstanding_pkts];
    // An array of ResendPacketCallbackArgs to pass to the ResendPacketCallback function
    // When a timer expires.
    struct ResendPacketCallbackArgs resend_pkt_cb_args[max_outstanding_pkts];

    uint64_t timer_prev_tsc = 0, timer_cur_tsc;

    // Initialize timers
    // And set args constant values (Values that do not change for the same worker thread)
    for (uint32_t i = 0; i < max_outstanding_pkts; i++) {
        rte_timer_init(&timers[i]);
        resend_pkt_cb_args[i].dwt = this;
        resend_pkt_cb_args[i].tx_mempool = tx_mempool;
        resend_pkt_cb_args[i].ppp = this->ppp_;
    }
#endif

    // The job slice struct that will be filled with the next job slice to work on.
    JobSlice job_slice;
    // Main worker thread loop
    // This is where optimization is very important
    while(ctx.GetContextState() == Context::ContextState::RUNNING) {
        // Get a job slice
        bool got_job_slice = ctx.GetJobSlice(this->tid_, job_slice);
        if(!got_job_slice) {
            continue;
        }
        DVLOG(2) << "Worker thread '" << this->tid_ << "' received job slice with job id: " << job_slice.job->id_ << " with numel: " << job_slice.slice.numel << ".";
        if(unlikely(job_slice.slice.numel <=0 || this->config_.general_.instant_job_completion)) {
            if(ctx.GetContextState() == Context::ContextState::RUNNING) {
                DVLOG(2) << "Worker thread '" << this->tid_ << "' notifying job slice completion with job id: " << job_slice.job->id_  << ".";
                ctx.NotifyJobSliceCompletion(this->tid_, job_slice);
            }
            continue;
        }

        // Setup the prepostprocessor and get the number of main packets that we will need to send.
        uint64_t total_num_pkts = this->ppp_->SetupJobSlice(&job_slice);

        // We can logically divide all of the packets that we will send into 'max_outstanding_pkts' sized groups
        // (Or less in case the total number of packets was less than max_outsanding_pkts).
        // We call each of these groups a batch. So if max_outstanding_pkts=10 and we wanted to send 70 packets then we have 7 batches.
        const uint64_t batch_num_pkts = std::min(max_outstanding_pkts, total_num_pkts);

        if(this->ppp_->NeedsExtraBatch()) {
            total_num_pkts += batch_num_pkts;
        }

        DVLOG(3) << "Worker thread '" << this->tid_ << "' will send a total of '" << total_num_pkts << "' packets";

        // Allocate a bitmap which will keep track of which packets have been received
        uint32_t bitmap_size = rte_bitmap_get_memory_footprint(total_num_pkts);
        LOG_IF(FATAL,  unlikely(bitmap_size == 0)) << "Worker thread '" << this->tid_ << "' Could not get the memory footprint of the bitmap";
        void* bitmap_mem = rte_zmalloc_socket("bitmap", bitmap_size, RTE_CACHE_LINE_SIZE, rte_socket_id());
        LOG_IF(FATAL, unlikely(bitmap_mem == NULL)) << "Worker thread '" << this->tid_ << "' Failed to allocate bitmap.";
        struct rte_bitmap* bitmap = rte_bitmap_init(total_num_pkts, static_cast<uint8_t*>(bitmap_mem), bitmap_size);
        LOG_IF(FATAL, unlikely(bitmap == NULL)) << "Worker thread '" << this->tid_ << "' Failed to init bitmap.";
        rte_bitmap_reset(bitmap);


        // Initialize statistic variables.
        // We found that using local variables for the inner loops speeds things up.
        // We then push those local variables to the context statistics object at the end of each job.
        uint64_t stats_wrong_pkts_received = 0;
        uint64_t stats_correct_pkts_received = 0;
        uint64_t stats_total_pkts_sent = 0;

#ifdef TIMEOUTS
        this->timer_cycles_ = initial_timer_cycles; // cycles for 1 ms
#endif

        // Create first batch of packets
        DVLOG(3) << "Worker thread '" << this->tid_ << "' creating first batch";
        uint64_t pkt_id = 0;
        for (uint64_t j = 0; j < batch_num_pkts; j++) {
            struct rte_mbuf* mbuf = pkts_tx_burst[j];

            // By default, when an mbuf is sent it is deallocated. However to avoid allocating
            // mbufs everytime a new job slice is received, we increase the refcnt of the mbuf.
            // now the mbuf will remain allocated even after it is sent and we can reuse it for
            // the next first batch in the next job slice.
            rte_mbuf_refcnt_update(mbuf,1);

            uint16_t switch_pool_index = PktId2PoolIndex(pkt_id,
                switch_pool_index_start, switch_pool_index_shift, max_outstanding_pkts);

            BuildPacket(mbuf, job_slice.job->id_, pkt_id, switch_pool_index, genconf.packet_numel,
                        bk.GetSwitchE2eAddr(), this->worker_thread_e2e_addr_be_, this->ppp_);
            pkt_id++;

#ifdef TIMEOUTS
            // The outstanding_pkt_index is just a way to associate each outstanding packet with a particular timer slot.
            // For example: suppose that max_outstanding_pkts was 10 (that means we have 10 timers) and we want to send 50 packets.
            // Then packet ids 0,10,20,30,40 will all use timer 0. But that is fine because we will not send packet 10 until packet 0 was recieved
            // and we will not send packet 20 until packet 10 has been received and so on.
            uint16_t outstanding_pkt_index = pkt_id % max_outstanding_pkts;

            // Update the callback arguments for this packet
            resend_pkt_cb_args[outstanding_pkt_index].job_id = job_slice.job->id_;
            resend_pkt_cb_args[outstanding_pkt_index].switch_pool_index = switch_pool_index;
            resend_pkt_cb_args[outstanding_pkt_index].pkt_id = pkt_id;

            // We start the timout with some extra time because creating and sending the first batch will take some time
            // We use the normal timer_cycles value later on.
            rte_timer_reset_sync(&timers[outstanding_pkt_index], this->timer_cycles_ * max_outstanding_pkts, PERIODICAL, this->lcore_id_,
                ResendPacketCallback, &resend_pkt_cb_args[outstanding_pkt_index]);
#endif
        }

        // Send first batch
        DVLOG(3) << "Worker thread '" << this->tid_ << "' sending first batch";
        uint16_t nb_tx;
        uint16_t num_sent_pkts = 0;
        do {
            nb_tx = rte_eth_tx_burst(dpdkconf.port_id, this->tid_, &pkts_tx_burst[num_sent_pkts], batch_num_pkts - num_sent_pkts);
            num_sent_pkts += nb_tx;
            DVLOG(3) << "Worker thread '" << this->tid_ << "' First batch sent " << nb_tx << "/" << batch_num_pkts << ".";
        } while (num_sent_pkts < batch_num_pkts);

        stats_total_pkts_sent += num_sent_pkts;

        // loop until all packets have been sent and received.
        // This is where optimization is MOST important.
        volatile uint32_t num_received_pkts = 0;
        uint16_t nb_rx;
        DVLOG(3) << "Worker thread '" << this->tid_ << "' entering receive send loop";
        while (likely(num_received_pkts < total_num_pkts && ctx.GetContextState() == Context::ContextState::RUNNING)) {
            // Read packet(s) from RX ring
            nb_rx = rte_eth_rx_burst(dpdkconf.port_id, this->tid_, pkts_rx_burst, dpdkconf.burst_rx);

            // Check if we should flush the tx buffer or retransmit anything.
            // We only do that if we haven't received any packets in this iteration
            // so that we give strict priority to processing received packets over
            // sending new ones.
            if (unlikely(nb_rx == 0)) {
                cur_tsc = rte_get_timer_cycles(); 
                if(unlikely(cur_tsc - prev_tsc > drain_tsc)) {
                    nb_tx = rte_eth_tx_buffer_flush(dpdkconf.port_id, this->tid_, tx_buffer);
                    prev_tsc = cur_tsc;
                    stats_total_pkts_sent += nb_tx;
                }

#ifdef TIMEOUTS
                // Check timers
                timer_cur_tsc = cur_tsc;
                if (unlikely(timer_cur_tsc - timer_prev_tsc > this->timer_cycles_)) {
                    rte_timer_manage();
                    timer_prev_tsc = timer_cur_tsc;
                }
#endif
                continue;
            }
            
            // Process all the packets that we've received.
            for (uint16_t j = 0; j < nb_rx; j++) {
                struct rte_mbuf* mbuf = pkts_rx_burst[j];
                pkts_rx_burst[j] = NULL; // The mbuf will be deallocated so we should discard its pointer.

                rte_prefetch0(rte_pktmbuf_mtod(mbuf, void *));

                struct rte_ether_hdr* ether = rte_pktmbuf_mtod(mbuf, struct rte_ether_hdr *);
                // Skip over ethernet, ip, and udp headers and get the switchml header.
                struct DpdkBackend::DpdkPacketHdr* switchml_hdr = 
                    (struct DpdkBackend::DpdkPacketHdr*) ( (uint8_t *) (ether+1) + sizeof(struct rte_ipv4_hdr) + sizeof(struct rte_udp_hdr) );

                uint32_t pkt_id = switchml_hdr->pkt_id;

                // Have we received this packet before ?
                if(unlikely(rte_bitmap_get(bitmap, pkt_id) == 1)) {
                    DVLOG(3) << "Worker thread '" << this->tid_ << "' Discarded duplicate packet short_job_id=" << (int) switchml_hdr->short_job_id
                        << " pkt_id=" << pkt_id;
                    rte_pktmbuf_free(mbuf);
                    stats_wrong_pkts_received++;
                    continue;
                }

                // Is this packet for the current running job ?
                if(unlikely(switchml_hdr->short_job_id != (uint8_t) job_slice.job->id_)) {
                    DVLOG(3) << "Worker thread '" << this->tid_ << "' Discarded packet from wrong job. short_job_id=" << (int) switchml_hdr->short_job_id
                        << " pkt_id=" << pkt_id;
                    rte_pktmbuf_free(mbuf);
                    stats_wrong_pkts_received++;
                    continue;
                }

                DVLOG(3) << "Worker thread '" << this->tid_ << "' Accepted packet short_job_id=" << (int) switchml_hdr->short_job_id
                    << " pkt_id=" << pkt_id;

                int16_t* extra_info_ptr = reinterpret_cast<int16_t*>(switchml_hdr+1);
                DpdkBackend::DpdkPacketElement* entries_ptr = reinterpret_cast<DpdkBackend::DpdkPacketElement*>(extra_info_ptr+1);
                this->ppp_->PostprocessSingle(pkt_id, entries_ptr, extra_info_ptr);

                num_received_pkts++;

                rte_bitmap_set(bitmap, pkt_id); // Mark this packet as received in the bitmap

                stats_correct_pkts_received++;

                // The next packet id of this mbuf (Or the packet that will use the same slot)
                pkt_id += batch_num_pkts;

#ifdef TIMEOUTS
                uint32_t outstanding_pkt_index = pkt_id % max_outstanding_pkts;
                rte_timer_stop_sync(&timers[outstanding_pkt_index]);
#endif

                // Free the mbuf and continue if there is no need to reuse it to send the next packet.
                if (unlikely(pkt_id >= total_num_pkts)) {
                    rte_pktmbuf_free(mbuf);
                    continue;
                }

                // Reuse the mbuf for the next packet
                DVLOG(3) << "Worker thread '" << this->tid_ << "' Reusing mbuf to send packet short_job_id=" << (int) switchml_hdr->short_job_id << " pkt_id=" << pkt_id;

                uint16_t switch_pool_index = PktId2PoolIndex(pkt_id, switch_pool_index_start, switch_pool_index_shift, max_outstanding_pkts);
                ReusePacket(mbuf, pkt_id, genconf.packet_numel, switch_pool_index, bk.GetSwitchE2eAddr(),
                            this->worker_thread_e2e_addr_be_, this->ppp_);

                // Send the packet
                nb_tx = rte_eth_tx_buffer(dpdkconf.port_id, this->tid_, tx_buffer, mbuf);
                if (nb_tx) {
                    stats_total_pkts_sent += nb_tx;
                    prev_tsc = cur_tsc;
                }
#ifdef TIMEOUTS
                // Update resend packet callback args
                resend_pkt_cb_args[outstanding_pkt_index].job_id = job_slice.job->id_;
                resend_pkt_cb_args[outstanding_pkt_index].switch_pool_index = switch_pool_index;
                resend_pkt_cb_args[outstanding_pkt_index].pkt_id = pkt_id;

                // Start timer for this reused packet
                // This call is responsible for the vast majority of the TIMEOUTS performance drop
                // trying the async version did not help. The drop is consistent even with 0 timeouts.
                rte_timer_reset_sync(&timers[outstanding_pkt_index], this->timer_cycles_, PERIODICAL, this->lcore_id_, ResendPacketCallback,
                        &resend_pkt_cb_args[outstanding_pkt_index]);
#endif
            } // for (uint16_t j = 0; j < nb_rx; j++)

            DVLOG(3) << "Worker thread '" << this->tid_ << "' received " << nb_rx << " packets. " << num_received_pkts << "/" << total_num_pkts << ".";

        } // while (num_received_pkts < total_num_pkts && ctx.GetContextState() == Context::ContextState::RUNNING)

        // Update switch shift for next job
        switch_pool_index_shift = (switch_pool_index_shift + total_num_pkts) % (2 * max_outstanding_pkts);

        rte_bitmap_free(bitmap);
        rte_free(bitmap_mem);

        this->ppp_->CleanupJobSlice();

        // Add the local stats to the context stats object.
        ctx.GetStats().AddCorrectPktsReceived(this->tid_, stats_correct_pkts_received);
        ctx.GetStats().AddTotalPktsSent(this->tid_, stats_total_pkts_sent);
        ctx.GetStats().AddWrongPktsReceived(this->tid_, stats_wrong_pkts_received);

        // Finally notify the ctx that the worker thread finished this job slice.
        // If the context exited then the notify call will simply fail and set the job to failed.
        DVLOG_IF(2, num_received_pkts == total_num_pkts) << "Worker thread '" << this->tid_ << "' notifying job slice completion with job id: " << job_slice.job->id_  << ".";
        ctx.NotifyJobSliceCompletion(this->tid_, job_slice);

    } // while(ctx.GetContextState() == Context::ContextState::RUNNING)

    // Cleanup
    rte_free(pkts_rx_burst);
    rte_pktmbuf_free_bulk(pkts_tx_burst, max_outstanding_pkts);
    rte_free(pkts_tx_burst);
    rte_free(tx_buffer);
    VLOG(0) << "Worker thread '" << this->tid_ << "' exiting.";
}

} // namespace switchml