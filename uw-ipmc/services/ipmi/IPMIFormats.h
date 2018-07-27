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

std::string render_ipmi_type_length_field(std::vector<uint8_t> &data);

#endif /* SRC_COMMON_UW_IPMC_SERVICES_IPMI_IPMIFORMATS_H_ */
