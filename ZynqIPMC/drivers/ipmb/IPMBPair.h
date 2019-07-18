/*
 * IPMBPair.h
 *
 *  Created on: Jul 26, 2018
 *      Author: jtikalsky
 */

#ifndef SRC_COMMON_UW_IPMC_DRIVERS_IPMB_IPMBPAIR_H_
#define SRC_COMMON_UW_IPMC_DRIVERS_IPMB_IPMBPAIR_H_

#include <drivers/generics/ipmb.h>
#include <libs/LogTree.h>

/**
 * An IPMB pair, bonded and used as a single IPMB, such as IPMB-0 in ATCA.
 */
class IPMBPair : public IPMB {
public:
	IPMB *ipmb[2];        ///< The IPMBs bonded into this configuration
	bool ipmb_enabled[2]; ///< Whether each IPMB of the pair is enabled.
	LogTree *log;         ///< An optional logtree for request dispatch tracking.

	/**
	 * Instantiate an IPMBPair, from two provided subordinate IPMBs.
	 *
	 * @param ipmbA Subordinate IPMB 0
	 * @param ipmbB Subordinate IPMB 1
	 * @param log   An optional logtree for request dispatch tracking.
	 */
	IPMBPair(IPMB *ipmbA, IPMB *ipmbB, LogTree *log=NULL)
		: IPMB(), ipmb{ipmbA, ipmbB}, ipmb_enabled{true, true}, log(log) { };
	virtual ~IPMBPair() { };

	/**
	 * Set the incoming message queue for this IPMBPair.
	 *
	 * @param incoming_message_queue The incoming message queue associated with this IPMBPair
	 */
	virtual void set_incoming_message_queue(QueueHandle_t incoming_message_queue) {
		this->incoming_message_queue = incoming_message_queue;
		this->ipmb[0]->set_incoming_message_queue(incoming_message_queue);
		this->ipmb[1]->set_incoming_message_queue(incoming_message_queue);
	}

	virtual bool send_message(IPMI_MSG &msg, uint32_t retry=0);
};

#endif /* SRC_COMMON_UW_IPMC_DRIVERS_IPMB_IPMBPAIR_H_ */
