/*
 * IPMBSvc.h
 *
 *  Created on: Dec 8, 2017
 *      Author: jtikalsky
 */

#ifndef SRC_COMMON_UW_IPMC_SERVICES_IPMI_IPMBSVC_IPMBSVC_H_
#define SRC_COMMON_UW_IPMC_SERVICES_IPMI_IPMBSVC_IPMBSVC_H_

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
#include <libs/LogTree.h>
#include <libs/SkyRoad.h>
#include <drivers/generics/IPMB.h>
#include <drivers/watchdog/PSWDT.h>
#include <services/ipmi/IPMI_MSG.h>
#include <services/ipmi/ipmbsvc/IPMICommandParser.h>
#include <services/console/CommandParser.h>
#include <services/ipmi/IPMI.h>

/**
 * An IPMBSvc driver.
 */
class IPMBSvc {
public:
	const u8 ipmb_address;  ///< The IPMB address of this node.
	const std::string name; ///< The name used for this IPMB in StatCounter or Task names, as well as log messages.
	IPMICommandParser *command_parser; ///< The command parser used for incoming messages.

	IPMBSvc(IPMB *ipmbA, IPMB *ipmbB, uint8_t ipmb_address, IPMICommandParser *command_parser, LogTree &logtree, const std::string name, PS_WDT *wdt);
	virtual ~IPMBSvc();

	static uint8_t lookup_ipmb_address(const int gpios[8]);

	/**
	 * The supplied function will be called when a response to this outgoing
	 * message is received, or when delivery is aborted.
	 *
	 * \note This will not be called for outgoing response messages except in
	 *       the case of inability to transmit.
	 *
	 * \param original The original IPMI_MSG transmitted.
	 * \param response The response IPMI_MSG received, or NULL if delivery aborted.
	 */
	typedef std::function<void(std::shared_ptr<IPMI_MSG> original, std::shared_ptr<IPMI_MSG> response)> response_cb_t;

	void send(std::shared_ptr<IPMI_MSG> msg, response_cb_t response_cb = NULL);
	std::shared_ptr<IPMI_MSG> send_sync(std::shared_ptr<IPMI_MSG> msg);

	LogTree &logroot; ///< The root logtree for this object.

	virtual void register_console_commands(CommandParser &parser, const std::string &prefix="");
	virtual void deregister_console_commands(CommandParser &parser, const std::string &prefix="");

protected:
	/**
	 * A record representing a message in the outgoing message queue.
	 */
	class IPMB_MsgRec {
	public:
		std::shared_ptr<IPMI_MSG> msg; ///< The message.
		response_cb_t response_cb;     ///< The response callback, used to report error or success.
		uint8_t retry_count;           ///< The current retry count.
		AbsoluteTimeout next_retry;    ///< The timeout for the next retry.
		/// Construct
		IPMB_MsgRec(std::shared_ptr<IPMI_MSG> &msg, response_cb_t response_cb = NULL) : msg(msg), response_cb(response_cb), retry_count(0), next_retry(0ul) { };
	};

	IPMB *ipmb[2]; ///< The subordinate IPMBs.
	const size_t recvq_size = 32; ///< The length of the receive queue.
	QueueHandle_t recvq; ///< A queue for received messages from both interfaces.
	SemaphoreHandle_t sendq_mutex; ///< A mutex protecting the sendq.
	SemaphoreHandle_t sendq_notify_sem; ///< A binary semaphore used to signal the task after a .send()
	StatCounter stat_recvq_highwater;    ///< A statistics counter for the high-water fill mark for the receive queue.
	StatCounter stat_sendq_highwater;    ///< A statistics counter for the high-water fill mark for the transmit queue.
	StatCounter stat_messages_received;  ///< How many messages have been received by this IPMB
	StatCounter stat_messages_delivered; ///< How many messages have been delivered successfully by this IPMB
	StatCounter stat_send_attempts;      ///< How many delivery attempts have been made by this IPMB
	StatCounter stat_send_failures;      ///< How many delivery failures have occurred on this IPMB
	StatCounter stat_no_available_seq;   ///< How many times we have run out of sequence numbers for a specific command on this IPMB
	StatCounter stat_unexpected_replies; ///< How many unexpected replies have been received on this IPMB
	LogTree &log_messages_in;            ///< Messages received on our IPMB.
	LogTree &log_messages_out;           ///< Messages transmitted on our IPMB.

	/**
	 * The number of attempts made to send a given IPMI message.
	 * This must not exceed IPMB spec Table 4-1, "Seq. no. expiration interval" in total duration.
	 */
	const uint8_t max_retries = 10;
	std::list<IPMB_MsgRec> outgoing_messages; ///< The queue of outgoing IPMI messages.
	TaskHandle_t task; ///< A reference to the IPMBSvc task owned by this object.
	QueueSetHandle_t qset; ///< A queueset for use in the thread task.

	PS_WDT *wdt; ///< The watchdog
	PS_WDT::slot_handle_t wdt_slot; ///< Our watchdog slot

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
	/**
	 * A record of incoming sequence numbers for commands.
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
	std::map<uint32_t, uint64_t> incoming_sequence_numbers;
	bool check_duplicate(const IPMI_MSG &msg);
public:
	void run_thread(); ///< \protected Run the IPMBSvc thread code.
};

#endif /* SRC_COMMON_UW_IPMC_DRIVERS_IPMBSVC_IPMBSVC_H_ */
