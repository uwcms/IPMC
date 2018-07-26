/*
 * IPMBPair.cpp
 *
 *  Created on: Jul 26, 2018
 *      Author: jtikalsky
 */

#include <drivers/ipmb/IPMBPair.h>
#include <IPMC.h>

/**
 * This function will send a message out on the correct IPMB in a blocking manner.
 *
 * The sub-IPMB used for a transmission is chosen by a rudimentary hash algorithm
 * and will be switched between retries of the same message.  If one IPMB is
 * disabled, it will not be used and all messages will be directed to the other.
 * If both IPMBs are disabled, this will return false immediately.
 *
 * \param msg   The IPMI_MSG to deliver.
 * \param retry The retry count for this message.
 * \return      true if message was delivered else false
 */
bool IPMBPair::send_message(IPMI_MSG &msg, uint32_t retry) {
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
		return this->ipmb[hash]->send_message(msg, retry);
	}

	// Or if that IPMB is not enabled, use the other.
	hash += 1;
	hash %= 2;

	if (this->ipmb_enabled[hash]) {
		if (this->log)
			this->log->log(stdsprintf("Dispatching message to secondary IPMB (%hhu): ", hash) + msg.format(), LogTree::LOG_DIAGNOSTIC);
		return this->ipmb[hash]->send_message(msg, retry);
	}

	// Or if both are disabled, fail.
	if (this->log)
		this->log->log(std::string("Unable to dispatch message (no IPMB enabled): ") + msg.format(), LogTree::LOG_INFO);
	return false;
}
