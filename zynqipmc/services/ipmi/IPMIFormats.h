/*
 * IPMIFormats.h
 *
 *  Created on: Jul 27, 2018
 *      Author: jtikalsky
 */

#ifndef SRC_COMMON_UW_IPMC_SERVICES_IPMI_IPMIFORMATS_H_
#define SRC_COMMON_UW_IPMC_SERVICES_IPMI_IPMIFORMATS_H_

#include <vector>
#include <string>
#include <stddef.h>

unsigned ipmi_type_length_field_get_length(const std::vector<uint8_t> &data);
std::string render_ipmi_type_length_field(const std::vector<uint8_t> &data);
std::vector<uint8_t> encode_ipmi_type_length_field(const std::string &data, bool prevent_c1 = false);

#endif /* SRC_COMMON_UW_IPMC_SERVICES_IPMI_IPMIFORMATS_H_ */
