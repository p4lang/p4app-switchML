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
        ("backend.dpdk.switch_port", po::value<uint16_t>(&this->backend_.dpdk.switch_port)->default_value(48879))
        ("backend.dpdk.switch_ip", po::value<std::string>(&this->backend_.dpdk.switch_ip_str)->required())
        ("backend.dpdk.switch_mac", po::value<std::string>(&this->backend_.dpdk.switch_mac_str)->required())

        ("backend.dpdk.cores", po::value<std::string>(&this->backend_.dpdk.cores_str)->default_value("0-2"))
        ("backend.dpdk.extra_eal_options", po::value<std::string>(&this->backend_.dpdk.extra_eal_options)->default_value(""))
        ("backend.dpdk.port_id", po::value<uint16_t>(&this->backend_.dpdk.port_id)->default_value(0))
        ("backend.dpdk.pool_size", po::value<uint32_t>(&this->backend_.dpdk.pool_size)->default_value(8192 * 32))
        ("backend.dpdk.pool_cache_size", po::value<uint32_t>(&this->backend_.dpdk.pool_cache_size)->default_value(256 * 2))
        ("backend.dpdk.burst_rx", po::value<uint32_t>(&this->backend_.dpdk.burst_rx)->default_value(64))
        ("backend.dpdk.burst_tx", po::value<uint32_t>(&this->backend_.dpdk.burst_tx)->default_value(64))
        ("backend.dpdk.bulk_drain_tx_us", po::value<uint32_t>(&this->backend_.dpdk.bulk_drain_tx_us)->default_value(100))
#ifdef TIMEOUTS
        ("backend.dpdk.timeout", po::value<double>(&this->backend_.dpdk.timeout)->default_value(10))
        ("backend.dpdk.timeout_threshold", po::value<uint64_t>(&this->backend_.dpdk.timeout_threshold)->default_value(50000))
        ("backend.dpdk.timeout_threshold_increment", po::value<uint64_t>(&this->backend_.dpdk.timeout_threshold_increment)->default_value(50000))
#endif
    ;
    config_file_options.add(dpdk_options);
#endif

#ifdef RDMA
    po::options_description rdma_options("backend.rdma");
    rdma_options.add_options()
        ("backend.rdma.controller_ip", po::value<std::string>(&this->backend_.rdma.controller_ip_str)->default_value("127.0.0.1"))
        ("backend.rdma.controller_port", po::value<uint16_t>(&this->backend_.rdma.controller_port)->default_value(50099))
        ("backend.rdma.msg_numel", po::value<uint32_t>(&this->backend_.rdma.msg_numel)->default_value(1024))
        ("backend.rdma.device_name", po::value<std::string>(&this->backend_.rdma.device_name)->default_value("mlx5_0"))
        ("backend.rdma.device_port_id", po::value<uint16_t>(&this->backend_.rdma.device_port_id)->default_value(1))
        ("backend.rdma.gid_index", po::value<uint16_t>(&this->backend_.rdma.gid_index)->default_value(3))
        ("backend.rdma.use_gdr", po::value<bool>(&this->backend_.rdma.use_gdr)->default_value(true))
#ifdef TIMEOUTS
        ("backend.rdma.timeout", po::value<double>(&this->backend_.rdma.timeout)->default_value(10))
        ("backend.rdma.timeout_threshold", po::value<uint64_t>(&this->backend_.rdma.timeout_threshold)->default_value(50000))
#endif
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
    // TODO: Add more checks. Maybe each backend should validate its own configuration as well.
    LOG_IF(FATAL, this->general_.max_outstanding_packets / this->general_.num_worker_threads == 0) 
        << "The chosen max_outstanding_packets must be at least equal to num_worker_threads to let each worker thread send at least 1 packet";

    LOG_IF(WARNING, this->general_.max_outstanding_packets % this->general_.num_worker_threads != 0)
        << "The chosen max_outstanding_packets is not divisible by num_worker_threads. It will be rounded down to the nearest divisible value. Bandwidth may be underutilized";

    if(this->general_.backend == "dpdk") {
        LOG_IF(FATAL, this->general_.packet_numel != 256 && this->general_.packet_numel != 64) << "The DPDK backend only supports 256 or 64 elements per packet. '" << this->general_.packet_numel << "' is not valid.";
    }
}

void Config::PrintConfig() {
    VLOG(0) << "Printing configuration";

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
    ;

#ifdef DUMMY
    VLOG_IF(0, this->general_.backend == "dummy") << "\n[backend.dummy]"
        << "\n    bandwidth = " << this->backend_.dummy.bandwidth
        << "\n    process_packets = " << this->backend_.dummy.process_packets
    ;
#endif

#ifdef DPDK
    VLOG_IF(0, this->general_.backend == "dpdk") << "[backend.dpdk]"
        << "\n    worker_port = " << this->backend_.dpdk.worker_port
        << "\n    worker_ip = " << this->backend_.dpdk.worker_ip_str
        << "\n    switch_port = " << this->backend_.dpdk.switch_port
        << "\n    switch_ip = " << this->backend_.dpdk.switch_ip_str
        << "\n    switch_mac = " << this->backend_.dpdk.switch_mac_str

        << "\n    cores = " << this->backend_.dpdk.cores_str
        << "\n    extra_eal_options = " << this->backend_.dpdk.extra_eal_options
        << "\n    port_id = " << this->backend_.dpdk.port_id
        << "\n    pool_size = " << this->backend_.dpdk.pool_size
        << "\n    pool_cache_size = " << this->backend_.dpdk.pool_cache_size
        << "\n    burst_rx = " << this->backend_.dpdk.burst_rx
        << "\n    burst_tx = " << this->backend_.dpdk.burst_tx
        << "\n    bulk_drain_tx_us = " << this->backend_.dpdk.port_id

#ifdef TIMEOUTS
        << "\n    timeout = " << this->backend_.dpdk.timeout
        << "\n    timeout_threshold = " << this->backend_.dpdk.timeout_threshold
        << "\n    timeout_threshold_increment = " << this->backend_.dpdk.timeout_threshold_increment
#endif
    ;
#endif

#ifdef RDMA
    VLOG_IF(0, this->general_.backend == "rdma") << "\n[backend.rdma]"
        << "\n    controller_ip_str = " << this->backend_.rdma.controller_ip_str
        << "\n    controller_port = " << this->backend_.rdma.controller_port
        << "\n    msg_numel = " << this->backend_.rdma.msg_numel
        << "\n    device_name = " << this->backend_.rdma.device_name
        << "\n    device_port_id = " << this->backend_.rdma.device_port_id
        << "\n    gid_index = " << this->backend_.rdma.gid_index
        << "\n    use_gdr = " << this->backend_.rdma.use_gdr
#ifdef TIMEOUTS
        << "\n    timeout = " << this->backend_.rdma.timeout
        << "\n    timeout_threshold = " << this->backend_.rdma.timeout_threshold
#endif
    ;
#endif
}

} // namespace switchml