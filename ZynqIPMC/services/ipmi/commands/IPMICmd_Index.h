#ifndef SRC_COMMON_UW_IPMC_SERVICES_IPMI_COMMANDS_IPMICMD_INDEX_H_
#define SRC_COMMON_UW_IPMC_SERVICES_IPMI_COMMANDS_IPMICMD_INDEX_H_

#include <services/ipmi/ipmbsvc/IPMICommandParser.h>

extern std::map<uint16_t, IPMICommandParser::ipmi_cmd_handler_t> *ipmicmd_index;

#define IPMICMD_INDEX_REGISTER(cmd) \
	static void _auto_ipmicmd_index_register_ ## cmd() __attribute__((constructor(1000))); \
	static void _auto_ipmicmd_index_register_ ## cmd() { \
		if (!ipmicmd_index) \
			ipmicmd_index = new std::map<uint16_t, IPMICommandParser::ipmi_cmd_handler_t>(); \
		(*ipmicmd_index)[IPMI::cmd] = ipmicmd_ ## cmd; \
	}

void ipmicmd_default(IPMBSvc &ipmb, const IPMI_MSG &message);

#endif /* SRC_COMMON_UW_IPMC_SERVICES_IPMI_COMMANDS_IPMICMD_INDEX_H_ */
