/*
 * IPMBDispatchRouter.h
 *
 *  Created on: Jul 26, 2018
 *      Author: jtikalsky
 */

#ifndef SRC_COMMON_UW_IPMC_DRIVERS_IPMB_IPMBDISPATCHROUTER_H_
#define SRC_COMMON_UW_IPMC_DRIVERS_IPMB_IPMBDISPATCHROUTER_H_

#include <drivers/generics/IPMB.h>
#include <libs/LogTree.h>
#include <map>

/**
 * An IPMB Dispatch Router, dispatching outgoing messages on different IPMBs
 * based on their target I2C address.  This is intended to support IPMB-L.
 */
class IPMBDispatchRouter : public IPMB {
public:
	std::map<uint8_t, IPMB*> routing_table; ///< The routing table for this dispatch router
	IPMB *default_route;                    ///< The target for all non-matched messages (or NULL to fail delivery)
	LogTree *log;                           ///< An optional logtree for request dispatch tracking.

	IPMBDispatchRouter(const std::map<uint8_t, IPMB*> &routing_table, IPMB* default_route=NULL, LogTree *log=NULL);

	virtual void set_incoming_message_queue(QueueHandle_t incoming_message_queue);

	virtual bool send_message(IPMI_MSG &msg, uint32_t retry=0);
};

#endif /* SRC_COMMON_UW_IPMC_DRIVERS_IPMB_IPMBDISPATCHROUTER_H_ */
