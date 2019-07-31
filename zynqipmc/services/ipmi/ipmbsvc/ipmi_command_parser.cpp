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

#include <FreeRTOS.h>
#include <libs/threading.h>
#include <semphr.h>
#include <services/ipmi/ipmbsvc/ipmi_command_parser.h>
#include <services/ipmi/ipmi_message.h>
#include <functional>
#include <map>

IPMICommandParser::IPMICommandParser(ipmi_cmd_handler_t default_handler, const std::map<uint16_t, ipmi_cmd_handler_t> commandset)
	: default_handler(default_handler), commandset(commandset) {
	this->mutex = xSemaphoreCreateMutex();
	configASSERT(this->mutex);
}

IPMICommandParser::~IPMICommandParser() {
	vSemaphoreDelete(this->mutex);
}

void IPMICommandParser::registerHandler(uint16_t command, ipmi_cmd_handler_t handler) {
	MutexGuard<false> lock(this->mutex, true);
	if (!handler) {
		this->commandset.erase(command);
	} else {
		this->commandset[command] = handler;
	}
}

void IPMICommandParser::dispatch(IPMBSvc &ipmb, const IPMIMessage &message) {
	ipmi_cmd_handler_t handler = this->default_handler;
	uint16_t command = (message.netFn << 8) | message.cmd;
	MutexGuard<false> lock(this->mutex, true);

	if (this->commandset.count(command))
		handler = this->commandset[command];

	lock.release();

	if (handler)
		handler(ipmb, message);
}
