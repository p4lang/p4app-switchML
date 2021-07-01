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
 * @brief Take a MAC address as an 8 bytes integer with the first 6 bytes representing the MAC address
 * and return the string representation of it.
 * 
 * @param addr an 8 bytes integer with the first 6 bytes representing the MAC address.
 * @return std::string the string represntation of the MAC address (FF:FF:FF:FF:FF:FF)
 */
std::string Mac2Str(const uint64_t addr);

/**
 * @brief Take a MAC address as an array of 6 bytes and return the string representation of it.
 * 
 * @param addr 6 byte array representing the MAC address
 * @return std::string the string represntation of the MAC address (FF:FF:FF:FF:FF:FF)
 */
std::string Mac2Str(const rte_ether_addr addr);

/**
 * @brief Take a string representation of the MAC address and return 8 bytes integer.
 * with the first 6 bytes representing the MAC address.
 * 
 * @param mac_str the string represntation of the MAC address (FF:FF:FF:FF:FF:FF)
 * @return an 8 bytes integer with the first 6 bytes representing the MAC address.
 */
uint64_t Str2Mac(std::string const& mac_str);

/**
 * @brief Take a MAC address as an 8 bytes integer with the first 6 bytes representing the MAC address
 * and convert its endianness.
 * 
 * @param mac an 8 bytes integer with the first 6 bytes representing the original MAC address.
 * @return uint64_t an 8 bytes integer with the first 6 bytes representing the converted MAC address.
 */
uint64_t ChangeMacEndianness(uint64_t mac);

} // namespace switchml
#endif // SWITCHML_DPDK_UTILS_H_