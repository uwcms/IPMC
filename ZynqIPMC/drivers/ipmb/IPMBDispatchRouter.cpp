/*
 * IPMBDispatchRouter.cpp
 *
 *  Created on: Jul 26, 2018
 *      Author: jtikalsky
 */

#include <drivers/ipmb/IPMBDispatchRouter.h>

/**
 * Instantiate an IPMB Dispatch Router
 *
 * @param routing_table The routing table (I2CAddr -> IPMB*) for this router
 * @param default_route The default route when there is no match in the table
 * @param log           An optional logtree for request dispatch tracking.
 */
IPMBDispatchRouter::IPMBDispatchRouter(const std::map<uint8_t, IPMB*> &routing_table, IPMB* default_route, LogTree *log)
	: routing_table(routing_table), default_route(default_route), log(log) {
}

/**
 * Set the incoming message queue for this IPMB.
 *
 * @param incoming_message_queue The incoming message queue associated with this IPMB
 */
void IPMBDispatchRouter::set_incoming_message_queue(QueueHandle_t incoming_message_queue) {
	this->incoming_message_queue = incoming_message_queue;
	for (auto it = this->routing_table.begin(), eit = this->routing_table.end(); it != eit; ++it)
		it->second->set_incoming_message_queue(incoming_message_queue);
	if (this->default_route)
		this->default_route->set_incoming_message_queue(incoming_message_queue);
}

/**
 * This function will send a message out on the correct IPMB in a blocking manner.
 *
 * The routing table will be consulted, and if no match is found, this will fall
 * back to the default route, or fail the delivery.
 *
 * \param msg   The IPMI_MSG to deliver.
 * \param retry The retry count for this message.
 * \return      true if message was delivered else false
 */
bool IPMBDispatchRouter::send_message(IPMI_MSG &msg, uint32_t retry) {
	// If the target is in our tables, dispatch to it.
	if (this->routing_table.count(msg.rsSA)) {
		if (this->log)
			this->log->log(std::string("Dispatching message via routing table: ") + msg.format(), LogTree::LOG_DIAGNOSTIC);
		return this->routing_table.at(msg.rsSA)->send_message(msg, retry);
	}

	// Otherwise, dispatch to the default route.
	if (this->default_route) {
		if (this->log)
			this->log->log(std::string("Dispatching message via default route: ") + msg.format(), LogTree::LOG_DIAGNOSTIC);
		return this->default_route->send_message(msg, retry);
	}

	// Otherwise, fail.
	if (this->log)
		this->log->log(std::string("Unable to dispatch message (no match or default route): ") + msg.format(), LogTree::LOG_NOTICE);
	return false;
}
