/**
 * SwitchML Project
 * @file config.cc
 * @brief Implements the Config class.
 */


#include "config.h"

#include <limits.h>

#include <fstream>
#include <boost/program_options.hpp>

#include "common_cc.h"

namespace po = boost::program_options;

namespace switchml {

void Config::operator=(Config const& other) {
    this->general_ = other.general_;
    this->backend_ = other.backend_;
}

bool Config::LoadFromFile(std::string path) {
    po::options_description config_file_options;
    // Add options to variable mappings
    po::options_description general_options("general");
    general_options.add_options() 
        ("general.rank", po::value<uint16_t>(&this->general_.rank)->default_value(0))
        ("general.num_workers", po::value<uint16_t>(&this->general_.num_workers)->default_value(1))
        ("general.num_worker_threads", po::value<uint16_t>(&this->general_.num_worker_threads)->default_value(4))
        ("general.max_outstanding_packets", po::value<uint32_t>(&this->general_.max_outstanding_packets)->default_value(256))
        ("general.packet_numel", po::value<uint64_t>(&this->general_.packet_numel)->default_value(1024))
        ("general.backend", po::value<std::string>(&this->general_.backend)->default_value("dummy"))
        ("general.scheduler", po::value<std::string>(&this->general_.scheduler)->default_value("fifo"))
        ("general.prepostprocessor", po::value<std::string>(&this->general_.prepostprocessor)->default_value("cpu_exponent_quantizer"))
        ("general.instant_job_completion", po::value<bool>(&this->general_.instant_job_completion)->default_value(false))
        ("general.controller_ip", po::value<std::string>(&this->general_.controller_ip_str)->default_value("127.0.0.1"))
        ("general.controller_port", po::value<uint16_t>(&this->general_.controller_port)->default_value(50099))
#ifdef TIMEOUTS
        ("general.timeout", po::value<double>(&this->general_.timeout)->default_value(10))
        ("general.timeout_threshold", po::value<uint64_t>(&this->general_.timeout_threshold)->default_value(100))
        ("general.timeout_threshold_increment", po::value<uint64_t>(&this->general_.timeout_threshold_increment)->default_value(100))
#endif
    ;
    config_file_options.add(general_options);

#ifdef DUMMY
    po::options_description dummy_options("backend.dummy");
    dummy_options.add_options()
        ("backend.dummy.bandwidth", po::value<float>(&this->backend_.dummy.bandwidth)->default_value(1000.0))
        ("backend.dummy.process_packets", po::value<bool>(&this->backend_.dummy.process_packets)->default_value(true))
    ;
    config_file_options.add(dummy_options);
#endif

#ifdef DPDK
    po::options_description dpdk_options("backend.dpdk");
    dpdk_options.add_options()
        ("backend.dpdk.worker_port", po::value<uint16_t>(&this->backend_.dpdk.worker_port)->default_value(4000))
        ("backend.dpdk.worker_ip", po::value<std::string>(&this->backend_.dpdk.worker_ip_str)->default_value("10.0.0.1"))
        ("backend.dpdk.cores", po::value<std::string>(&this->backend_.dpdk.cores_str)->default_value("0-2"))
        ("backend.dpdk.extra_eal_options", po::value<std::string>(&this->backend_.dpdk.extra_eal_options)->default_value(""))
        ("backend.dpdk.port_id", po::value<uint16_t>(&this->backend_.dpdk.port_id)->default_value(0))
        ("backend.dpdk.pool_size", po::value<uint32_t>(&this->backend_.dpdk.pool_size)->default_value(8192 * 32))
        ("backend.dpdk.pool_cache_size", po::value<uint32_t>(&this->backend_.dpdk.pool_cache_size)->default_value(256 * 2))
        ("backend.dpdk.burst_rx", po::value<uint32_t>(&this->backend_.dpdk.burst_rx)->default_value(64))
        ("backend.dpdk.burst_tx", po::value<uint32_t>(&this->backend_.dpdk.burst_tx)->default_value(64))
        ("backend.dpdk.bulk_drain_tx_us", po::value<uint32_t>(&this->backend_.dpdk.bulk_drain_tx_us)->default_value(100))
    ;
    config_file_options.add(dpdk_options);
#endif

#ifdef RDMA
    po::options_description rdma_options("backend.rdma");
    rdma_options.add_options()
        ("backend.rdma.msg_numel", po::value<uint32_t>(&this->backend_.rdma.msg_numel)->default_value(1024))
        ("backend.rdma.device_name", po::value<std::string>(&this->backend_.rdma.device_name)->default_value("mlx5_0"))
        ("backend.rdma.device_port_id", po::value<uint16_t>(&this->backend_.rdma.device_port_id)->default_value(1))
        ("backend.rdma.gid_index", po::value<uint16_t>(&this->backend_.rdma.gid_index)->default_value(3))
        ("backend.rdma.use_gdr", po::value<bool>(&this->backend_.rdma.use_gdr)->default_value(true))
    ;
    config_file_options.add(rdma_options);
#endif

    // Open configuration file
    std::ifstream ifs;
    if(!path.empty()) {
        // If a path was provided, open it.
        ifs.open(path.c_str());
        if(!ifs.good()){
            ifs.close();
            LOG(WARNING) << "The configuration file '" << path << "' is not valid";
            return false;
        }
    } else {
        // If no path was provided then find a default configuration file
        std::string default_path1 = "/etc/switchml.cfg";
        ifs.open(default_path1.c_str());
        if(!ifs.good()){
            ifs.close();

            char hostname[HOST_NAME_MAX+1];
            if (gethostname(hostname,sizeof(hostname))!=0) {
                LOG(FATAL) << "gethostname failed: "+ std::string(strerror(errno));
            }
            std::string default_path2 = "switchml-"+std::string(hostname)+".cfg";
            ifs.open(default_path2.c_str());
            if(!ifs.good()){
                ifs.close();
                
                std::string default_path3 = "switchml.cfg";
                ifs.open(default_path3.c_str());
                if(!ifs.good()){
                    ifs.close();
                    LOG(WARNING) << "No configuration file found in '" << default_path1 << "', '" << default_path2 << "', or '" << default_path3 << "'.";
                    return false;
                } else {
                    VLOG(0) << "Using this configuration file '" << default_path3 << "'.";
                }
            } else {
                VLOG(0) << "Using this configuration file '" << default_path2 << "'.";
            }
        } else {
            VLOG(0) << "Using this configuration file '" << default_path1 << "'.";
        }
    }

    // Read and parse the configuration file
    po::variables_map vm;
    po::store(po::parse_config_file(ifs, config_file_options), vm);
    po::notify(vm);

    return true;
}

void Config::Validate() {
    // TODO: Add more checks. Maybe each backend should validate its own configuration instead of doing it here?.
    LOG_IF(FATAL, this->general_.max_outstanding_packets / this->general_.num_worker_threads == 0) 
        << "The chosen max_outstanding_packets must be at least equal to num_worker_threads to let each worker thread send at least 1 packet";

    uint64_t outstanding_pkts_per_wt = this->general_.max_outstanding_packets / this->general_.num_worker_threads;
    uint64_t valid_mop = outstanding_pkts_per_wt*this->general_.num_worker_threads;
    if (valid_mop != this->general_.max_outstanding_packets) {
        LOG(WARNING) << "general.max_outstanding_packets '" << this->general_.max_outstanding_packets << "' is not divisible by general.num_worker_threads '"
            << this->general_.num_worker_threads << ".\n"
            << "Setting it to '" << valid_mop << "'."
        ;
        this->general_.max_outstanding_packets = valid_mop;
    }

#ifdef DPDK
    if(this->general_.backend == "dpdk") {
        LOG_IF(FATAL, this->general_.packet_numel != 256 && this->general_.packet_numel != 64) 
            << "The DPDK backend only supports 256 or 64 elements per packet. '" << this->general_.packet_numel << "' is not valid.";
    }
#endif
#ifdef RDMA
    if(this->general_.backend == "rdma") {
        LOG_IF(FATAL, this->general_.packet_numel != 256 && this->general_.packet_numel != 64) 
            << "The RDMA backend only supports 256 or 64 elements per packet. '" << this->general_.packet_numel << "' is not valid.";
        
        LOG_IF(FATAL, this->backend_.rdma.msg_numel < this->general_.packet_numel) << "rdma.msg_numel cannot be less than general.packet_numel.";

        uint64_t num_pkts_per_msg = this->backend_.rdma.msg_numel/this->general_.packet_numel;
        if (this->backend_.rdma.msg_numel % this->general_.packet_numel != 0) {
            uint64_t new_msg_numel = num_pkts_per_msg * this->general_.packet_numel;
            LOG(WARNING) << "rdma.msg_numel '" << this->backend_.rdma.msg_numel << "' is not divisible by general.packet_numel '"
                << this->general_.packet_numel << "'. We will set rdma.msg_numel to '" << new_msg_numel << "'.";
            this->backend_.rdma.msg_numel = new_msg_numel;
        }
        uint64_t outstanding_msgs = this->general_.max_outstanding_packets / num_pkts_per_msg;
        uint64_t outstanding_msgs_per_wt = outstanding_msgs / this->general_.num_worker_threads;
        valid_mop = outstanding_msgs_per_wt * this->general_.num_worker_threads * num_pkts_per_msg;
        if(valid_mop != this->general_.max_outstanding_packets) {
            LOG(WARNING) << "general.max_outstanding_packets '" << this->general_.max_outstanding_packets << "' is not divisible by '" 
                << this->general_.num_worker_threads * num_pkts_per_msg 
                << "' (number of packets per message * number of worker threads).\n"
                << ". We will set general.max_outstanding_packets to '" << valid_mop << "' to have exactly " << outstanding_msgs_per_wt 
                << " outstanding messages per worker thread."
            ;
            this->general_.max_outstanding_packets = valid_mop;
        }
    }
#endif
}

void Config::PrintConfig() {
    VLOG(0) << "Printing configuration";

    uint64_t outstanding_pkts_per_wt = this->general_.max_outstanding_packets / this->general_.num_worker_threads;
    VLOG(0) << "\n[general]"
        << "\n    rank = " << this->general_.rank
        << "\n    num_workers = " << this->general_.num_workers
        << "\n    num_worker_threads = " << this->general_.num_worker_threads
        << "\n    max_outstanding_packets = " << this->general_.max_outstanding_packets
        << "\n    packet_numel = " << this->general_.packet_numel
        << "\n    backend = " << this->general_.backend
        << "\n    scheduler = " << this->general_.scheduler
        << "\n    prepostprocessor = " << this->general_.prepostprocessor
        << "\n    instant_job_completion = " << this->general_.instant_job_completion
        << "\n    controller_ip_str = " << this->general_.controller_ip_str
        << "\n    controller_port = " << this->general_.controller_port
#ifdef TIMEOUTS
        << "\n    timeout = " << this->general_.timeout
        << "\n    timeout_threshold = " << this->general_.timeout_threshold
        << "\n    timeout_threshold_increment = " << this->general_.timeout_threshold_increment
#endif
        << "\n    --(derived)--"
        << "\n    max_outstanding_packets_per_worker_thread = " <<  outstanding_pkts_per_wt
    ;

#ifdef DUMMY
    VLOG_IF(0, this->general_.backend == "dummy") << "\n[backend.dummy]"
        << "\n    bandwidth = " << this->backend_.dummy.bandwidth
        << "\n    process_packets = " << this->backend_.dummy.process_packets
    ;
#endif

#ifdef DPDK
    if(this->general_.backend == "dpdk") {
        VLOG(0) << "\n[backend.dpdk]"
            << "\n    worker_port = " << this->backend_.dpdk.worker_port
            << "\n    worker_ip = " << this->backend_.dpdk.worker_ip_str

            << "\n    cores = " << this->backend_.dpdk.cores_str
            << "\n    extra_eal_options = " << this->backend_.dpdk.extra_eal_options
            << "\n    port_id = " << this->backend_.dpdk.port_id
            << "\n    pool_size = " << this->backend_.dpdk.pool_size
            << "\n    pool_cache_size = " << this->backend_.dpdk.pool_cache_size
            << "\n    burst_rx = " << this->backend_.dpdk.burst_rx
            << "\n    burst_tx = " << this->backend_.dpdk.burst_tx
            << "\n    bulk_drain_tx_us = " << this->backend_.dpdk.port_id
        ;
    }
#endif

#ifdef RDMA
    if(this->general_.backend == "rdma") {
        uint64_t num_pkts_per_msg = this->backend_.rdma.msg_numel/this->general_.packet_numel;
        uint64_t outstanding_msgs = this->general_.max_outstanding_packets / num_pkts_per_msg;
        uint64_t outstanding_msgs_per_wt = outstanding_msgs / this->general_.num_worker_threads;
        VLOG(0) << "\n[backend.rdma]"
            << "\n    msg_numel = " << this->backend_.rdma.msg_numel
            << "\n    device_name = " << this->backend_.rdma.device_name
            << "\n    device_port_id = " << this->backend_.rdma.device_port_id
            << "\n    gid_index = " << this->backend_.rdma.gid_index
            << "\n    use_gdr = " << this->backend_.rdma.use_gdr

            << "\n    --(derived)--"
            << "\n    num_pkts_per_msg = " <<  num_pkts_per_msg
            << "\n    max_outstanding_msgs = " << outstanding_msgs
            << "\n    max_outstanding_msgs_per_worker_thread = " << outstanding_msgs_per_wt
        ;
    }
#endif
}

} // namespace switchml