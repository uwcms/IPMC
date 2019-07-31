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

#include "ipmb_dispatch_router.h"

IPMBDispatchRouter::IPMBDispatchRouter(const std::map<uint8_t, IPMB*> &routing_table, IPMB* default_route, LogTree *log)
	: routing_table(routing_table), default_route(default_route), log(log) {
}

void IPMBDispatchRouter::setIncomingMessageQueue(QueueHandle_t incoming_message_queue) {
	this->incoming_message_queue = incoming_message_queue;

	for (auto it = this->routing_table.begin(), eit = this->routing_table.end(); it != eit; ++it)
		it->second->setIncomingMessageQueue(incoming_message_queue);

	if (this->default_route)
		this->default_route->setIncomingMessageQueue(incoming_message_queue);
}

bool IPMBDispatchRouter::sendMessage(IPMIMessage &msg, uint32_t retry) {
	// If the target is in our tables, dispatch to it.
	if (this->routing_table.count(msg.rsSA)) {
		if (this->log)
			this->log->log(std::string("Dispatching message via routing table: ") + msg.format(), LogTree::LOG_DIAGNOSTIC);
		return this->routing_table.at(msg.rsSA)->sendMessage(msg, retry);
	}

	// Otherwise, dispatch to the default route.
	if (this->default_route) {
		if (this->log)
			this->log->log(std::string("Dispatching message via default route: ") + msg.format(), LogTree::LOG_DIAGNOSTIC);
		return this->default_route->sendMessage(msg, retry);
	}

	// Otherwise, fail.
	if (this->log) {
		this->log->log(std::string("Unable to dispatch message (no match or default route): ") + msg.format(), LogTree::LOG_NOTICE);
	}

	return false;
}
