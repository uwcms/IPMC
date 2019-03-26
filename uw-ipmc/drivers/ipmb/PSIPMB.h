/*
 * PSIPMBAB.h
 *
 *  Created on: Oct 24, 2017
 *      Author: jtikalsky
 */

#ifndef SRC_COMMON_UW_IPMC_DRIVERS_PS_IPMB_AB_PSIPMB_H_
#define SRC_COMMON_UW_IPMC_DRIVERS_PS_IPMB_AB_PSIPMB_H_

#include <functional>
#include "xiicps.h"
#include <FreeRTOS.h>
#include <semphr.h>
#include <queue.h>
#include <IPMC.h>
#include <libs/RingBuffer.h>
#include <libs/ThreadingPrimitives.h>
#include <libs/StatCounter.h>
#include <drivers/generics/IPMB.h>
#include <drivers/InterruptBasedDriver.h>
#include <services/ipmi/IPMI_MSG.h>
#include <services/ipmi/ipmbsvc/IPMBSvc.h>

/**
 * An interrupt-based driver for the PS I2C, specialized for IPMB functionality.
 */
class PS_IPMB : public IPMB, protected InterruptBasedDriver {
public:
	const u8 IPMBAddr; ///< The local IPMB slave address.

	PS_IPMB(u16 DeviceId, u32 IntrId, u8 IPMBAddr);
	virtual ~PS_IPMB();

	virtual bool send_message(IPMI_MSG &msg, uint32_t retry=0);

	StatCounter messages_received;         ///< The number of messages received on this IPMB.
	StatCounter invalid_messages_received; ///< The number of received messages on this IPMB that are discarded as invalid.
	StatCounter incoming_messages_missed;  ///< The number of received messages on this IPMB that are discarded for lack of space or readiness.
	StatCounter unexpected_send_result_interrupts; ///< The number of unexpected send result interrupts we have received.

protected:
	static const int i2c_bufsize = 40; ///< The buffer size for I2C interactions. (Must be 1 greater than needed.)
	volatile bool master;              ///< Identify whether the IPMB is currently in a master or slave mode.
	XIicPs IicInst;                    ///< The I2C driver instance handle.
	u8 i2c_inbuf[i2c_bufsize];         ///< The buffer for incoming I2C data.
	SemaphoreHandle_t mutex;           ///< A mutex serializing IPMB message requests.
	QueueHandle_t sendresult_q;        ///< A queue to transfer the Send result from ISR land back to the send() function.

	static void _InterruptPassthrough(PS_IPMB *ps_ipmb, u32 StatusEvent);
	void _InterruptHandler();                    ///< \protected Internal.
	void _VariableLengthSlaveInterruptHandler(); ///< \protected Internal.
	virtual void _HandleStatus(u32 StatusEvent); ///< \protected Internal.

	virtual void setup_slave();
	virtual void setup_master();
};

#endif /* SRC_COMMON_UW_IPMC_DRIVERS_PS_IPMB_AB_PSIPMB_H_ */
