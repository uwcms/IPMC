/*
 * IPMICommandParser.cpp
 *
 *  Created on: Jan 9, 2018
 *      Author: jtikalsky
 */

#include <services/ipmi/ipmbsvc/IPMICommandParser.h>
#include <FreeRTOS.h>
#include <semphr.h>
#include <IPMC.h>
#include <services/ipmi/IPMI_MSG.h>
#include <functional>
#include <map>

/**
 * Instantiate an IPMI command parser.
 *
 * @param default_handler Called when an unknown command is received. (or NULL)
 */
IPMICommandParser::IPMICommandParser(ipmi_cmd_handler_t default_handler)
	: default_handler(default_handler) {
	this->mutex = xSemaphoreCreateMutex();
	configASSERT(this->mutex);
}

IPMICommandParser::~IPMICommandParser() {
	vSemaphoreDelete(this->mutex);
}


/**
 * Sets the handler for a given IPMI command, or NULL to delete it.
 *
 * @param command The command as MSB=NetFn, LSB=Cmd
 * @param handler The handler for the command
 */
void IPMICommandParser::register_handler(uint16_t command, ipmi_cmd_handler_t handler) {
	xSemaphoreTake(this->mutex, portMAX_DELAY);
	if (!handler)
		this->commandset.erase(command);
	else
		this->commandset[command] = handler;
	xSemaphoreGive(this->mutex);
}

/**
 * Parse an IPMI command and dispatch it to the appropriate handler.
 *
 * @param ipmb The IPMB this command came in through
 * @param message The IPMI_MSG received
 */
void IPMICommandParser::dispatch(IPMBSvc &ipmb, const IPMI_MSG &message) {
	ipmi_cmd_handler_t handler = this->default_handler;
	uint16_t command = (message.netFn << 8) | message.cmd;
	xSemaphoreTake(this->mutex, portMAX_DELAY);
	if (this->commandset.count(command))
		handler = this->commandset[command];
	xSemaphoreGive(this->mutex);
	if (handler)
		handler(ipmb, message);
}
