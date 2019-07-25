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

#include <libs/logtree/logtree.h>

#include <IPMC.h>
#include <libs/printf.h>
#include <services/ipmi/ipmbsvc/ipmbsvc.h>
#include <services/ipmi/IPMI.h>

void ipmicmd_default(IPMBSvc &ipmb, const IPMI_MSG &message) {
	LogTree &logtree = ipmb.logroot["unknown_commands"];

	u16 cmd = (message.netFn<<8) | message.cmd;
	if (IPMI::id_to_cmd.count(cmd)) {
		auto cmdname = IPMI::id_to_cmd.at(cmd);
		logtree.log(stdsprintf("Unimplemented IPMI command (%s: %s) received: %s", cmdname.first.c_str(), cmdname.second.c_str(), message.format().c_str()), LogTree::LOG_NOTICE);
	} else {
		logtree.log(stdsprintf("Unimplemented and unknown IPMI command received: %s", message.format().c_str()), LogTree::LOG_NOTICE);
	}

	ipmb.send(message.prepare_reply({IPMI::Completion::Invalid_Command}));
}
