/**
 * SwitchML Project
 * @file dpdk_utils.cc
 * @brief Implements various dpdk related utility classes and functions.
 */

#include "dpdk_utils.h"

#include <iomanip>

#include "common_cc.h"

namespace switchml {

template<typename T>
std::string ToHex(T i) {
    std::stringstream stream;
    stream << std::setfill('0') << std::setw(sizeof(T) * 2) << std::hex << (int) i;
    return stream.str();
}
template std::string ToHex<int>(int);
template std::string ToHex<uint64_t>(uint64_t);

std::string Mac2Str(const rte_ether_addr addr) {

    std::string mac = "";
    for (int i = 0; i < 5; i++) {
        mac += ToHex(addr.addr_bytes[i]) + ":";
    }

    return mac + ToHex(addr.addr_bytes[5]);
}

struct rte_ether_addr Str2Mac(std::string const& s, bool change_endianess) {
    struct rte_ether_addr mac;
    uint8_t* ptr = mac.addr_bytes;
    uint last;
    int rc;

    if (change_endianess)
        rc = sscanf(s.c_str(), "%hhx:%hhx:%hhx:%hhx:%hhx:%hhx%n", ptr + 5, ptr + 4, ptr + 3, ptr + 2, ptr + 1, ptr, &last);
    else
        rc = sscanf(s.c_str(), "%hhx:%hhx:%hhx:%hhx:%hhx:%hhx%n", ptr, ptr + 1, ptr + 2, ptr + 3, ptr + 4, ptr + 5, &last);

    LOG_IF(FATAL, rc != 6 || s.size() != last) << "Failed to parse mac address '" << s << "'.";
    return mac;
}

} // namespace switchml