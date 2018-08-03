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
std::vector<std::string> stringSplit(std::string str, char delimiter);

#endif /* SRC_COMMON_UW_IPMC_LIBS_UTILS_H_ */
