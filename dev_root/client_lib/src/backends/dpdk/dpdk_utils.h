/**
 * SwitchML Project
 * @file dpdk_utils.h
 * @brief Declares various dpdk related utility classes and functions.
 */

#include <string>
#include <rte_ether.h>

#include "common.h"

#ifndef SWITCHML_DPDK_UTILS_H_
#define SWITCHML_DPDK_UTILS_H_

namespace switchml {

template<typename T>
/**
 * @brief Takes a data element and returns the hex string that represents its bits.
 * 
 * @return std::string the hex string of the bytes stored in the passed data element
 */
std::string ToHex(T);

/**
 * @brief Take a mac address as an array of bytes and return the string representation of it.
 * 
 * @param addr the array of bytes representing the mac address
 * @return std::string the string represntation of the mac address (FF:FF:FF:FF:FF:FF)
 */
std::string Mac2Str(const rte_ether_addr addr);

/**
 * @brief Take a string representation of the mac address and return an array of bytes.
 * 
 * @param mac_str the string represntation of the mac address (FF:FF:FF:FF:FF:FF)
 * @param change_endianess Controls whether you want to change the endianness when converting.
 * @return struct rte_ether_addr the array of bytes representing the mac address
 */
struct rte_ether_addr Str2Mac(std::string const& mac_str, bool change_endianess);

} // namespace switchml
#endif // SWITCHML_DPDK_UTILS_H_