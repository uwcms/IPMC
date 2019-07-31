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

#ifndef SRC_COMMON_ZYNQIPMC_DRIVERS_PSIPMB_PSIPMB_H_
#define SRC_COMMON_ZYNQIPMC_DRIVERS_PSIPMB_PSIPMB_H_

// Only include driver if the PS IIC is detected in the BSP.
#if XSDK_INDEXING || __has_include("xiicps.h")

#include "xiicps.h"
#include <functional>
#include <FreeRTOS.h>
#include <semphr.h>
#include <queue.h>
#include <libs/ringbuffer.h>
#include <libs/threading.h>
#include <libs/statcounter/statcounter.h>
#include <drivers/generics/ipmb.h>
#include <drivers/interrupt_based_driver.h>
#include <services/ipmi/ipmi_message.h>
#include <services/ipmi/ipmbsvc/ipmbsvc.h>

/**
 * An interrupt-based driver for the PS I2C, specialized for IPMB functionality.
 */
class PSIPMB : public IPMB, protected InterruptBasedDriver {
public:
	/**
	 * Instantiate a PSIPMB driver.
	 *
	 * @note This performs hardware setup (mainly interrupt configuration).
	 * @param device_id Device ID, normally XPAR_PS7_I2C_<>_DEVICE_ID. Check xparameters.h.
	 * @param intr_id Device interrupt ID from GIC, normally XPAR_PS7_I2C_<>_INTR. Check xparameters.h.
	 * @param addr The IPMB slave address to listen on.
	 */
	PSIPMB(uint16_t device_id, uint16_t intr_id, uint8_t addr);
	virtual ~PSIPMB();

	virtual bool sendMessage(IPMIMessage &msg, uint32_t retry = 0);

protected:
	static const int i2c_bufsize = 40;	///< Buffer size for I2C interactions. (Must be 1 greater than needed.)
	volatile bool master;				///< Identify whether the IPMB is currently in a master or slave mode.
	XIicPs iic;							///< I2C driver instance handle.
	const uint8_t kIPMBAddr;			///< Local IPMB slave address.
	uint8_t i2c_inbuf[i2c_bufsize];		///< Buffer for incoming I2C data.
	SemaphoreHandle_t mutex;			///< A mutex serializing IPMB message requests.
	QueueHandle_t sendresult_q;			///< A queue to transfer the Send result from ISR land back to the send() function.

	static void _InterruptPassthrough(PSIPMB *ps_ipmb, uint32_t StatusEvent);
	virtual void _InterruptHandler();					///< \protected Internal.
	void _VariableLengthSlaveInterruptHandler();		///< \protected Internal.
	virtual void _HandleStatus(uint32_t StatusEvent);	///< \protected Internal.

	virtual void setupSlave(); ///< Configure the device in slave mode and initiate receiving.
	virtual void setupMaster(); ///< Configure the device in master mode.

private:
	StatCounter messages_received;         ///< The number of messages received on this IPMB.
	StatCounter invalid_messages_received; ///< The number of received messages on this IPMB that are discarded as invalid.
	StatCounter incoming_messages_missed;  ///< The number of received messages on this IPMB that are discarded for lack of space or readiness.
	StatCounter unexpected_send_result_interrupts; ///< The number of unexpected send result interrupts we have received.
	StatCounter lost_transmit_interrutpts; ///< The number of transmit complete interrupts that never arrived.
};

#endif

#endif /* SRC_COMMON_ZYNQIPMC_DRIVERS_PSIPMB_PSIPMB_H_ */
