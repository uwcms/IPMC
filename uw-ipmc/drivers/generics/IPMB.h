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

	/**
	 * This queue of typename IPMI_MSG receives deliveries of incoming IPMB
	 * messages from this interface, if not NULL.
	 */
	QueueHandle_t incoming_message_queue;

	/**
	 * This function will send a message out on the IPMB in a blocking manner.
	 *
	 * \param msg  The IPMI_MSG to deliver.
	 * \return     true if message was delivered else false
	 */
	virtual bool send_message(IPMI_MSG &msg) = 0;
};

#endif /* SRC_COMMON_UW_IPMC_DRIVERS_GENERICS_IPMB_H_ */
