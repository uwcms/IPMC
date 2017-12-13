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

/**
 * An IPMB Message record.
 *
 * This contains an IPMI command, complete with all relevant headers for IPMB-0
 * transit, as well as parsing and construction functions.
 */
class IPMI_MSG {
public:
	static const u8 max_data_len = 32; ///< The max length of command data.  // TODO: Verify this is <32
	u8 rsSA;               ///< (byte 0)       The responder slave address.
	u8 netFn;              ///< (byte 1[7:2])  The network function of the command.
	u8 rsLUN;              ///< (byte 1[1:0])  The responder LUN.
	// hdr_sum;            ///< (byte 2)       The header checksum.
	u8 rqSA;               ///< (byte 3)       The requester slave address.
	u8 rqSeq;              ///< (byte 4[7:2])  The request sequence number.
	u8 rqLUN;              ///< (byte 4[1:0])  The requester LUN.
	u8 cmd;                ///< (byte 5)       The IPMI command number.
	u8 data[max_data_len]; ///< (byte 6-*)     The IPMI command parameter/response data.
	u8 data_len;           ///<                The length of the parameter/response data.
	// all_sum;            ///< (byte last)    The message checksum.

	bool parse_message(u8 *msg, u8 len);
	int unparse_message(u8 *msg, u8 maxlen) const;
	void prepare_reply(IPMI_MSG &reply) const;
	bool match(const IPMI_MSG &other) const;
	bool match_reply(const IPMI_MSG &other) const;
};

/**
 * An interrupt-based driver for the PS I2C, specialized for IPMB functionality.
 */
class PS_IPMB {
public:
	PS_IPMB(u16 DeviceId, u32 IntrId, u8 SlaveAddr);
	virtual ~PS_IPMB();
	void _HandleInterrupt(u32 StatusEvent); ///< \protected Internal.

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
	bool send_message(IPMI_MSG &msg);

	StatCounter messages_received;         ///< The number of messages received on this IPMB.
	StatCounter invalid_messages_received; ///< The number of received messages on this IPMB that are discarded as invalid.
	StatCounter incoming_messages_missed;  ///< The number of received messages on this IPMB that are discarded for lack of space or readiness.
	StatCounter unexpected_send_result_interrupts; ///< The number of unexpected send result interrupts we have received.
protected:
	static const int i2c_bufsize = 40; ///< The buffer size for I2C interactions. (Must be 1 greater than needed.)
	bool master;                       ///< Identify whether the IPMB is currently in a master or slave mode.
	XIicPs IicInst;                    ///< The I2C driver instance handle.
	u8 SlaveAddr;                      ///< The local IPMB slave address.
	u8 i2c_inbuf[i2c_bufsize];         ///< The buffer for incoming I2C data.
	u32 i2c_result;                    ///< The result of the I2C operation.
	SemaphoreHandle_t mutex;           ///< A mutex serializing IPMB message requests.
	u32 IntrId;                        ///< Interrupt ID, used to disable interrupts in the destructor.
	QueueHandle_t sendresult_q;        ///< A queue to transfer the Send result from ISR land back to the send() function.

	void setup_slave();
	void setup_master();
};

#endif /* SRC_COMMON_UW_IPMC_DRIVERS_PS_IPMB_AB_PSIPMB_H_ */
