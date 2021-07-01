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

std::string Mac2Str(const uint64_t addr) {

    std::string mac_str = "", hex;

    hex = ToHex((addr & ((uint64_t) 0xFF << 40)) >> 40);
    mac_str += hex.substr(sizeof(uint64_t) * 2 - 2, 2);

    for (int i = 4; i >= 0; i++) {

        hex = ToHex((addr & ((uint64_t) 0xFF << 8 * i)) >> 8 * i);
        mac_str += ":" + hex.substr(sizeof(uint64_t) * 2 - 2, 2);
    }

    return mac_str;
}

std::string Mac2Str(const rte_ether_addr addr) {

    std::string mac = "";
    for (int i = 0; i < 5; i++) {
        mac += ToHex(addr.addr_bytes[i]) + ":";
    }

    return mac + ToHex(addr.addr_bytes[5]);
}

uint64_t Str2Mac(std::string const& s) {
    uint8_t mac[6];
    uint last;
    int rc;

    rc = sscanf(s.c_str(), "%hhx:%hhx:%hhx:%hhx:%hhx:%hhx%n", mac, mac + 1, mac + 2, mac + 3, mac + 4, mac + 5, &last);

    if (rc != 6 || s.size() != last)
        return -1;
    else
        return uint64_t(mac[0]) << 40 | uint64_t(mac[1]) << 32 | uint64_t(mac[2]) << 24 | uint64_t(mac[3]) << 16 | uint64_t(mac[4]) << 8 | uint64_t(mac[5]);
}

uint64_t ChangeMacEndianness(uint64_t mac) {
    return (mac & 0xFFL) << 40 | (mac & 0xFF00L) << 24 | (mac & 0xFF0000L) << 8 | (mac & 0xFF000000L) >> 8 |
           (mac & 0xFF00000000L) >> 24 | (mac & 0xFF0000000000L) >> 40;
}

} // namespace switchml