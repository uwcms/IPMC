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

#include "ipmb_pair.h"
#include <libs/printf.h>

void IPMBPair::setIncomingMessageQueue(QueueHandle_t incoming_message_queue) {
	this->incoming_message_queue = incoming_message_queue;
	this->ipmb[0]->setIncomingMessageQueue(incoming_message_queue);
	this->ipmb[1]->setIncomingMessageQueue(incoming_message_queue);
}

bool IPMBPair::sendMessage(IPMI_MSG &msg, uint32_t retry) {
	uint8_t hash = 0;
	hash += msg.rsSA + msg.rsLUN;
	hash += msg.rqSA + msg.rqLUN + msg.rqSeq;
	hash += msg.netFn + msg.cmd;
	hash += retry;
	hash %= 2; // Select the IPMB.

	// If the target IPMB is enabled, send it.
	if (this->ipmb_enabled[hash]) {
		if (this->log)
			this->log->log(stdsprintf("Dispatching message to preferred IPMB (%hhu): ", hash) + msg.format(), LogTree::LOG_DIAGNOSTIC);
		return this->ipmb[hash]->sendMessage(msg, retry);
	}

	// Or if that IPMB is not enabled, use the other.
	hash += 1;
	hash %= 2;

	if (this->ipmb_enabled[hash]) {
		if (this->log)
			this->log->log(stdsprintf("Dispatching message to secondary IPMB (%hhu): ", hash) + msg.format(), LogTree::LOG_DIAGNOSTIC);
		return this->ipmb[hash]->sendMessage(msg, retry);
	}

	// Or if both are disabled, fail.
	if (this->log) {
		this->log->log(std::string("Unable to dispatch message (no IPMB enabled): ") + msg.format(), LogTree::LOG_INFO);
	}

	return false;
}
