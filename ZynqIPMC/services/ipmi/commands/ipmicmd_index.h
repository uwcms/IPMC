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

#ifndef SRC_COMMON_ZYNQIPMC_SERVICES_IPMI_COMMANDS_IPMICMD_INDEX_H_
#define SRC_COMMON_ZYNQIPMC_SERVICES_IPMI_COMMANDS_IPMICMD_INDEX_H_

#include <services/ipmi/ipmbsvc/ipmi_command_parser.h>

//! Mapping of all supported IPMI commands.
extern std::map<uint16_t, IPMICommandParser::ipmi_cmd_handler_t> *ipmicmd_index;

//! Register an IPMI command.
#define IPMICMD_INDEX_REGISTER(cmd) \
	static void _auto_ipmicmd_index_register_ ## cmd() __attribute__((constructor(1000))); \
	static void _auto_ipmicmd_index_register_ ## cmd() { \
		if (!ipmicmd_index) \
			ipmicmd_index = new std::map<uint16_t, IPMICommandParser::ipmi_cmd_handler_t>(); \
		(*ipmicmd_index)[IPMI::cmd] = ipmicmd_ ## cmd; \
	}

//! Default IPMI command when it is not supported.
void ipmicmd_default(IPMBSvc &ipmb, const IPMI_MSG &message);

#endif /* SRC_COMMON_ZYNQIPMC_SERVICES_IPMI_COMMANDS_IPMICMD_INDEX_H_ */
