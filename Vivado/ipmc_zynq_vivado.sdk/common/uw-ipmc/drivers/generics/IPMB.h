/*
 * IPMB.h
 *
 *  Created on: Jan 8, 2018
 *      Author: jtikalsky
 */

#ifndef SRC_COMMON_UW_IPMC_DRIVERS_GENERICS_IPMB_H_
#define SRC_COMMON_UW_IPMC_DRIVERS_GENERICS_IPMB_H_

#include <services/ipmi/IPMI_MSG.h>
#include <FreeRTOS.h>
#include <queue.h>
#include <xil_types.h>

/**
 * A generic abstract interface for an IPMB driver.
 */
class IPMB {
public:
	IPMB() : incoming_message_queue(NULL) { };
	virtual ~IPMB() { };

protected:
	/**
	 * This queue of typename IPMI_MSG receives deliveries of incoming IPMB
	 * messages from this interface, if not NULL.
	 */
	QueueHandle_t incoming_message_queue;
public:
	/**
	 * Set the incoming message queue for this IPMB.
	 *
	 * @param incoming_message_queue The incoming message queue associated with this IPMB
	 */
	virtual void set_incoming_message_queue(QueueHandle_t incoming_message_queue) {
		this->incoming_message_queue = incoming_message_queue;
	}
	/**
	 * Get the incoming message queue for this IPMB.
	 *
	 * @return The incoming message queue associated with this IPMB
	 */
	virtual QueueHandle_t get_incoming_message_queue() {
		return this->incoming_message_queue;
	}

	/**
	 * This function will send a message out on the IPMB in a blocking manner.
	 *
	 * \param msg   The IPMI_MSG to deliver.
	 * \param retry The retry counter for this message.
	 * \return      true if message was delivered else false
	 */
	virtual bool send_message(IPMI_MSG &msg, uint32_t retry=0) { configASSERT(0); return false; };
};

#endif /* SRC_COMMON_UW_IPMC_DRIVERS_GENERICS_IPMB_H_ */
