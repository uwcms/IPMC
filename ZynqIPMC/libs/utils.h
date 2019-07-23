/*
 * This file is part of the ZYNQ-IPMC Framework.
 *
 * The ZYNQ-IPMC Framework is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * The ZYNQ-IPMC Framework is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with the ZYNQ-IPMC Framework.  If not, see <https://www.gnu.org/licenses/>.
 */

/**
 * Library contains several misc quality of life utility functions.
 */

#ifndef SRC_COMMON_ZYNQIPMC_LIBS_UTILS_H_
#define SRC_COMMON_ZYNQIPMC_LIBS_UTILS_H_

#include <string>
#include <vector>

/**
 * Converts a total number of bytes into a formated string with B, KiB, MiB, GiB or TiB as suffix.
 * @param bytes Total number of bytes.
 * @return Formated string (e.g. '4.02 MiB').
 */
std::string bytesToString(uint64_t bytes);

/**
 * Split a string separated by a delimiter into several substrings.
 * @param str The string to split.
 * @param delimiter A single character delimiter.
 * @return A vector with all the substrings in it.
 */
std::vector<std::string> stringSplit(const std::string& str, char delimiter);

/**
 * Generate a formated print of a given data buffer.
 * @param ptr Pointer to the buffer.
 * @param bytes Number of bytes to print.
 * @param str_offset Offset in the data buffer.
 * @return Formated print of the buffer.
 */
std::string formatedHexString(const void *ptr, size_t bytes, size_t str_offset = 0);

// TODO: I don't think this function is required, there should be an alternative already somewhere!
/**
 * Parse a binary (b) or hex string (0x) to unsigned integer.
 * @param s String to parse.
 * @param v Parsed unsigned integer.
 * @return false if failed to parse string.
 */
bool toUint32(std::string &s, uint32_t &v);

#endif /* SRC_COMMON_ZYNQIPMC_LIBS_UTILS_H_ */
