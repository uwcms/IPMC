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

#ifndef SRC_COMMON_ZYNQIPMC_DRIVERS_GENERICS_IPMB_H_
#define SRC_COMMON_ZYNQIPMC_DRIVERS_GENERICS_IPMB_H_

#include <FreeRTOS.h>
#include <queue.h>
#include <xil_types.h>
#include <services/ipmi/IPMI_MSG.h>

/**
 * A generic abstract interface for an IPMB driver.
 */
class IPMB {
public:
	IPMB() : incoming_message_queue(NULL) { };
	virtual ~IPMB() { };

public:
	/**
	 * Set the incoming message queue for this IPMB.
	 *
	 * @param incoming_message_queue The incoming message queue associated with this IPMB
	 */
	virtual void setIncomingMessageQueue(QueueHandle_t incoming_message_queue) {
		this->incoming_message_queue = incoming_message_queue;
	}

	/**
	 * Get the incoming message queue for this IPMB.
	 *
	 * @return The incoming message queue associated with this IPMB
	 */
	virtual QueueHandle_t getIncomingMessageQueue() {
		return this->incoming_message_queue;
	}

	/**
	 * This function will send a message out on the IPMB in a blocking manner.
	 *
	 * @param msg   The IPMI_MSG to deliver.
	 * @param retry The retry counter for this message.
	 * @return      true if message was delivered else false
	 */
	virtual bool sendMessage(IPMI_MSG &msg, uint32_t retry = 0) = 0;

protected:
	/**
	 * This queue of typename IPMI_MSG receives deliveries of incoming IPMB
	 * messages from this interface, if not NULL.
	 */
	QueueHandle_t incoming_message_queue;
};

#endif /* SRC_COMMON_ZYNQIPMC_DRIVERS_GENERICS_IPMB_H_ */
