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

#ifndef SRC_COMMON_ZYNQIPMC_DRIVERS_IPMB_IPMB_DISPATCH_ROUTER_H_
#define SRC_COMMON_ZYNQIPMC_DRIVERS_IPMB_IPMB_DISPATCH_ROUTER_H_

#include <map>
#include <drivers/generics/ipmb.h>
#include <libs/logtree/logtree.h>

/**
 * An IPMB Dispatch Router, dispatching outgoing messages on different IPMBs
 * based on their target I2C address.  This is intended to support IPMB-L.
 */
class IPMBDispatchRouter : public IPMB {
public:
	/**
	 * Instantiate an IPMB Dispatch Router
	 *
	 * @param routing_table The routing table (I2CAddr -> IPMB*) for this router.
	 * @param default_route The default route when there is no match in the table.
	 * @param log           An optional logtree for request dispatch tracking.
	 */
	IPMBDispatchRouter(const std::map<uint8_t, IPMB*> &routing_table, IPMB* default_route=NULL, LogTree *log=NULL);

	virtual void setIncomingMessageQueue(QueueHandle_t incoming_message_queue);

	/**
	 * This function will send a message out on the correct IPMB in a blocking manner.
	 *
	 * The routing table will be consulted, and if no match is found, this will fall
	 * back to the default route, or fail the delivery.
	 *
	 * @param msg   The IPMIMessage to deliver.
	 * @param retry The retry count for this message.
	 * @return      true if message was delivered else false.
	 */
	virtual bool sendMessage(IPMIMessage &msg, uint32_t retry=0);

private:
	std::map<uint8_t, IPMB*> routing_table;	///< The routing table for this dispatch router
	IPMB *default_route;					///< The target for all non-matched messages (or NULL to fail delivery)
	LogTree *log;							///< An optional logtree for request dispatch tracking.
};

#endif /* SRC_COMMON_ZYNQIPMC_DRIVERS_IPMB_IPMB_DISPATCH_ROUTER_H_ */
