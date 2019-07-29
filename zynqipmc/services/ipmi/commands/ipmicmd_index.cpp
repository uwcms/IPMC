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

#include <services/ipmi/commands/ipmicmd_index.h>

std::map<uint16_t, IPMICommandParser::ipmi_cmd_handler_t> *ipmicmd_index;

static void ipmicmd_index_init_ifempty() __attribute__((constructor(1000)));
static void ipmicmd_index_init_ifempty() {
	if (!ipmicmd_index)
		ipmicmd_index = new std::map<uint16_t, IPMICommandParser::ipmi_cmd_handler_t>();
}
