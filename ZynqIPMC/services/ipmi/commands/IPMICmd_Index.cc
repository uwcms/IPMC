#include "IPMICmd_Index.h"

std::map<uint16_t, IPMICommandParser::ipmi_cmd_handler_t> *ipmicmd_index;

static void ipmicmd_index_init_ifempty() __attribute__((constructor(1000)));
static void ipmicmd_index_init_ifempty() {
	if (!ipmicmd_index)
		ipmicmd_index = new std::map<uint16_t, IPMICommandParser::ipmi_cmd_handler_t>();
}
