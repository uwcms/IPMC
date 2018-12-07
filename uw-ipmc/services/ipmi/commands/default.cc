#include <IPMC.h>
#include <libs/LogTree.h>
#include <libs/printf.h>
#include <services/ipmi/IPMI.h>
#include <services/ipmi/ipmbsvc/IPMBSvc.h>

void ipmicmd_default(IPMBSvc &ipmb, const IPMI_MSG &message) {
	LogTree &logtree = ipmb.logroot["unknown_commands"];

	u16 cmd = (message.netFn<<8) | message.cmd;
	if (IPMI::id_to_cmd.count(cmd)) {
		auto cmdname = IPMI::id_to_cmd.at(cmd);
		logtree.log(stdsprintf("Unimplemented IPMI command (%s: %s) received: %s", cmdname.first.c_str(), cmdname.second.c_str(), message.format().c_str()), LogTree::LOG_NOTICE);
	}
	else {
		logtree.log(stdsprintf("Unimplemented and unknown IPMI command received: %s", message.format().c_str()), LogTree::LOG_NOTICE);
	}

	ipmb.send(message.prepare_reply({IPMI::Completion::Invalid_Command}));
}
