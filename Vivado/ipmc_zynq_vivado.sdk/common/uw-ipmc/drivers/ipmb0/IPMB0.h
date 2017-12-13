/*
 * IPMB0.h
 *
 *  Created on: Dec 8, 2017
 *      Author: jtikalsky
 */

#ifndef SRC_COMMON_UW_IPMC_DRIVERS_IPMB0_IPMB0_H_
#define SRC_COMMON_UW_IPMC_DRIVERS_IPMB0_IPMB0_H_

#include <functional>
#include <list>
#include <FreeRTOS.h>
#include <semphr.h>
#include <queue.h>
#include <task.h>
#include <IPMC.h>
#include <libs/RingBuffer.h>
#include <libs/ThreadingPrimitives.h>
#include <libs/StatCounter.h>
#include <libs/SkyRoad.h>
#include <drivers/ps_ipmb/PSIPMB.h>

class IPMB0 {
public:
	IPMB0(PS_IPMB *ipmbA, PS_IPMB *ipmbB, uint8_t ipmb_address);
	virtual ~IPMB0();

	static uint8_t lookup_ipmb_address(int mio_gpio_base);

	SkyRoad::Messenger<const IPMI_MSG> * const ipmb_incoming; ///< A messenger topic to broadcast received messages on.
	SkyRoad::Messenger<const IPMI_MSG> * const ipmb_outgoing; ///< A messenger topic to listen for messages to transmit on.
	SkyRoad::Messenger<const IPMI_MSG> * const ipmb_outgoing_failure; ///< A messenger topic to broadcast failed message transmissions on.

protected:
	PS_IPMB *ipmb[2]; ///< The subordinate IPMBs.
	u8 ipmb_address;  ///< The IPMB address of this node.
	const size_t recvq_size = 8; ///< The length of the receive queue.
	QueueHandle_t recvq; ///< A queue for received messages from both interfaces.
	StatCounter stat_recvq_highwater; ///< A statistics counter for the high-water fill mark for the receive queue.
	StatCounter stat_sendq_highwater; ///< A statistics counter for the high-water fill mark for the transmit queue.
	StatCounter stat_messages_received; ///< How many messages have been received by this IPMB
	StatCounter stat_messages_sent;     ///< How many messages have been delivered successfully by this IPMB
	StatCounter stat_send_attempts;     ///< How many delivery attempts have been made by this IPMB
	StatCounter stat_send_failures;     ///< How many delivery failures have occurred on this IPMB
	StatCounter stat_no_available_seq;  ///< How many times we have run out of sequence numbers for a specific command on this IPMB
	StatCounter stat_unexpected_replies; ///< How many unexpected replies have been received on this IPMB

	/**
	 * A record representing a message in the outgoing message queue.
	 */
	class IPMB_MsgRec {
	public:
		IPMI_MSG msg;               ///< The message.
		uint8_t retry_count;        ///< The current retry count.
		AbsoluteTimeout next_retry; ///< The timeout for the next retry.
		/// Construct
		IPMB_MsgRec(IPMI_MSG &msg) : msg(msg), retry_count(0), next_retry(0ul) { };
	};

	/**
	 * The number of attempts made to send a given IPMI message.
	 * This must not exceed IPMB spec Table 4-1, "Seq. no. expiration interval" in total duration.
	 */
	const uint8_t max_retries = 10;
	std::list<IPMB_MsgRec> outgoing_messages; ///< The queue of outgoing IPMI messages.
	TaskHandle_t task; ///< A reference to the IPMB0 task owned by this object.
	QueueSetHandle_t qset; ///< A queueset for use in the thread task.
	const size_t temple_size = 8; ///< How long the incoming message queue should be for the SkyRoad::Temple

	/**
	 * A record of used sequence numbers for commands.
	 *
	 * The mapping key should be:
	 * xxyyzzss:
	 *   xx: The remote IPMB address
	 *   yy: The NetFn
	 *   zz: The Command
	 *   ss: The sequence number used
	 *
	 * The mapping value is the tick64 at which the sequence number was used.
	 */
	std::map<uint32_t, uint64_t> used_sequence_numbers;
	bool set_sequence(IPMI_MSG &msg);
public:
	void run_thread(); ///< \protected Run the IPMB0 thread code.
};

#endif /* SRC_COMMON_UW_IPMC_DRIVERS_IPMB0_IPMB0_H_ */
