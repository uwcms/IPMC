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

#ifndef SRC_COMMON_ZYNQIPMC_SERVICES_IPMI_IPMIFORMATS_H_
#define SRC_COMMON_ZYNQIPMC_SERVICES_IPMI_IPMIFORMATS_H_

#include <vector>
#include <string>
#include <stddef.h>

/**
 * Perform basic validation on the provided IPMI Type/Length field, and return
 * the length in bytes of the full raw field.
 *
 * @param data The Type/Length field.
 * @return 0 if invalid, else the length in bytes, including the header.
 */
unsigned getIpmiTypeLengthFieldLength(const std::vector<uint8_t> &data);

/**
 * Unpack and render a field specified in Type/Length byte format, as
 * specified by the Platform Management FRU Information Storage Definition v1.0.
 *
 * @param data The field data including the type/length code byte.
 * @return The rendered data in human readable format.
 */
std::string renderIpmiTypeLengthField(const std::vector<uint8_t> &data);

/**
 * A poor man's type-length encoder.  Only encodes raw ASCII.
 * @param data The string to encode.
 * @param prevent_c1 0xC1 tends to be 'end of record list'.  Add a space to the end of single-character fields.
 * @return The encoded data.
 */
std::vector<uint8_t> encodeIpmiTypeLengthField(const std::string &data, bool prevent_c1 = false);

#endif /* SRC_COMMON_ZYNQIPMC_SERVICES_IPMI_IPMIFORMATS_H_ */
