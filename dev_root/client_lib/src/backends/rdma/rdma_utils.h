/**
 * SwitchML Project
 * @file rdma_utils.h
 * @brief Declares and implements various rdma related utility classes and functions.
 */

#ifndef SWITCHML_RDMA_UTILS_H_
#define SWITCHML_RDMA_UTILS_H_

#include <infiniband/verbs.h>

#include <iostream>
#include <memory>
#include <string>
#include <vector>
#include <unordered_map>
#include <bits/stdc++.h> 
#include <glog/logging.h>
#include <cstdint>

#include <utils.h>

namespace switchml {

/**
 * @brief Extract the IPv4 address from a GID address.
 * 
 * @param [in] gid GID to extract from.
 * @return uint32_t The IP address as a 32 bit integer.
 * 
 * @see IPv4ToGID()
 */
inline uint32_t GIDToIPv4(const ibv_gid gid) {
  uint32_t ip = 0;
  ip |= gid.raw[12];
  ip <<= 8;
  ip |= gid.raw[13];
  ip <<= 8;
  ip |= gid.raw[14];
  ip <<= 8;
  ip |= gid.raw[15];
  return ip;
}

/**
 * @brief Extract the MAC address from a GID address.
 * 
 * @param [in] gid GID to extract from.
 * @return uint64_t The MAC address as a 64 bit integer (The two most significant bytes are ignored).
 * 
 * @see MACToGID()
 */
inline uint64_t GIDToMAC(const ibv_gid gid) {
  uint64_t mac = 0;
  mac |= gid.raw[8] ^ 2;
  mac <<= 8;
  mac |= gid.raw[9];
  mac <<= 8;
  mac |= gid.raw[10];
  mac <<= 8;
  mac |= gid.raw[13];
  mac <<= 8;
  mac |= gid.raw[14];
  mac <<= 8;
  mac |= gid.raw[15];
  return mac;
}

/**
 * @brief Create GID from IPv4 address.
 * 
 * @param [in] ip IP address as a 32 bit integer.
 * @return ibv_gid The created GID address.
 * 
 * @see GIDToIPv4()
 */
inline ibv_gid IPv4ToGID(const int32_t ip) {
  ibv_gid gid;
  gid.global.subnet_prefix = 0;
  gid.global.interface_id = 0;
  gid.raw[10] = 0xff;
  gid.raw[11] = 0xff;
  gid.raw[12] = (ip >> 24) & 0xff;
  gid.raw[13] = (ip >> 16) & 0xff;
  gid.raw[14] = (ip >> 8) & 0xff;
  gid.raw[15] = (ip >> 0) & 0xff;
  return gid;
}

/**
 * @brief Create GID from MAC address.
 * 
 * @param [in] mac Mac address as a 64 bit integer (The two most significant bytes are ignored).
 * @return ibv_gid The created GID address.
 * 
 * @see GIDToMAC()
 */
inline ibv_gid MACToGID(const uint64_t mac) {
  ibv_gid gid;
  gid.global.subnet_prefix = 0x80fe;
  gid.global.interface_id = 0;
  gid.raw[8] = ((mac >> 40) & 0xff) ^ 2;
  gid.raw[9] = (mac >> 32) & 0xff;
  gid.raw[10] = (mac >> 24) & 0xff;
  gid.raw[11] = 0xff;
  gid.raw[12] = 0xfe;
  gid.raw[13] = (mac >> 16) & 0xff;
  gid.raw[14] = (mac >> 8) & 0xff;
  gid.raw[15] = (mac >> 0) & 0xff;
  return gid;
}

/**
 * @brief A function to query the system and get all Core ids grouped up by their NUMA nodes.
 * 
 * @return std::unordered_map<int, std::vector<int>> A map with the NUMA node as the key and a vector
 * of physical core ids that reside on that numa node.
 */
inline std::unordered_map<int, std::vector<int>> GetCoresNuma() {
    std::unordered_map<int, std::vector<int>> result;
    std::string cmd_output = Execute("lscpu --parse=NODE,CORE"); // type lscpu --help for a full list of available columns.
    std::stringstream ss(cmd_output);
    std::string cmd_output_line;
    while(std::getline(ss, cmd_output_line, '\n')) {
        if(cmd_output_line[0] != '#') {
            std::stringstream element_ss(cmd_output_line);
            std::string element;
            std::vector<int> line_elements;
            while(std::getline(element_ss, element, ',')) {
                line_elements.push_back(std::stoi(element));
            }
            int numa_node = line_elements[0];
            int cpu = line_elements[1];
            result[numa_node].push_back(cpu);
        }
    }
    // Remove duplicates from core vectors
    auto it = result.begin();
    while(it != result.end()) {
        std::vector<int>& core_list = (*it).second;
        std::sort(core_list.begin(), core_list.end());
        core_list.erase(std::unique(core_list.begin(), core_list.end()), core_list.end());
        it++;
    }
    return result;
}

/**
 * @brief Query the system to find the NUMA node on which the given device resides.
 * 
 * @param [in] device 
 * @return int the NUMA node which this ibverbs device resides on.
 */
inline int GetDeviceNuma(ibv_device* device) {
    std::string cmd_output = Execute( ( "cat /sys/class/infiniband/" + std::string(ibv_get_device_name(device)) + "/device/numa_node" ).c_str() );
    int numa_node = std::stoi(cmd_output);
    return numa_node;
}


/**
 * @brief A function to bind the calling thread to an appropriate core.
 * 
 * @param worker_id 
 */
inline void BindToCore(ibv_device* device, uint32_t worker_id) {
    std::unordered_map<int, std::vector<int>> cores_numa = GetCoresNuma();
    int device_numa = GetDeviceNuma(device);

    if(worker_id > cores_numa[device_numa].size()) {
        LOG(FATAL) << "The request to bind to a cpu cannot be fulfiled for worker " << worker_id 
        << " all cpus that are on the same numa node '" << device_numa << "' as the device are taken by other workers.";
        // TODO: fall back to using other cores instead of exiting.
    }

    int chosen_core = cores_numa[device_numa][worker_id];
    VLOG(1) << "Worker " << worker_id << " bound to core " << chosen_core << " on NUMA node " << device_numa;

    CHECK_LT(chosen_core, sysconf(_SC_NPROCESSORS_ONLN))
        << "Requested more worker threads than available cores";

    cpu_set_t mask;
    CPU_ZERO(&mask);
    CPU_SET(chosen_core, &mask);

    PCHECK(sched_setaffinity(0, sizeof(mask), &mask) >= 0)
        << "Core binding failed";
}

} // namespace switchml

#endif // SWITCHML_RDMA_UTILS_H_