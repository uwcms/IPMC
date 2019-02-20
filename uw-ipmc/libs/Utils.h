/*
 * Utils.h
 *
 *  Created on: Aug 3, 2018
 *      Author: mpv
 */

#ifndef SRC_COMMON_UW_IPMC_LIBS_UTILS_H_
#define SRC_COMMON_UW_IPMC_LIBS_UTILS_H_

#include <string>
#include <vector>

/**
 * Converts a total number of bytes into a formated string with B, KiB, MiB, GiB or TiB as suffix.
 * @param bytes Total number of bytes
 * @return Formated string (e.g. '4.02 MiB')
 */
std::string bytesToString(uint64_t bytes);

/**
 * Split a string separated by a delimiter into several substrings.
 * @param str The string to split
 * @param delimiter A single character delimiter
 * @return A vector with all the substrings in it
 */
std::vector<std::string> stringSplit(const std::string& str, char delimiter);

/**
 * Generate a formated print of a given data buffer.
 * @param ptr Pointer to the buffer
 * @param bytes Number of bytes to print
 * @param str_offset Offset in the data buffer
 * @return Formated print of the buffer
 */
std::string formatedHexString(const void *ptr, size_t bytes, size_t str_offset = 0);

/**
 * Parse a binary (b) or hex string (0x) to unsigned integer.
 * @param s String to parse
 * @param v Parsed unsigned integer
 * @return false if failed to parse string
 */
bool toUint32(std::string &s, uint32_t &v);

#endif /* SRC_COMMON_UW_IPMC_LIBS_UTILS_H_ */
