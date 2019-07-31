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

#ifndef SRC_COMMON_ZYNQIPMC_SERVICES_IPMI_IPMBSVC_IPMBSVC_H_
#define SRC_COMMON_ZYNQIPMC_SERVICES_IPMI_IPMBSVC_IPMBSVC_H_

#include <drivers/generics/ipmb.h>
#include <functional>
#include <list>
#include <FreeRTOS.h>
#include <semphr.h>
#include <queue.h>
#include <task.h>
#include <drivers/watchdog/ps_wdt.h>
#include <libs/logtree/logtree.h>
#include <libs/ringbuffer.h>
#include <libs/statcounter/statcounter.h>
#include <libs/threading.h>
#include <services/console/command_parser.h>
#include <services/ipmi/ipmbsvc/ipmi_command_parser.h>
#include <services/ipmi/ipmi.h>
#include <services/ipmi/ipmi_message.h>

/**
 * An IPMBSvc driver.
 */
class IPMBSvc final : public ConsoleCommandSupport {
public:

	/**
	 * Instantiate the IPMB service.
	 *
	 * @param ipmb           The underlying IPMB.
	 * @param ipmb_address   The IPMB address of this node.
	 * @param command_parser The command parser for incoming commands.
	 * @param logtree        The logtree for messages from the IPMBSvc.
	 * @param name           Used for StatCounter, Messenger and thread name.
	 * @param wdt            The watchdog instance to register & service.
	 */
	IPMBSvc(IPMB &ipmb, uint8_t ipmb_address, IPMICommandParser *command_parser, LogTree &logtree, const std::string name, PSWDT *wdt, bool wait_for_service_init = true);
	virtual ~IPMBSvc();

	/**
	 * The supplied function will be called when a response to this outgoing
	 * message is received, or when delivery is aborted.
	 *
	 * @note This will not be called for outgoing response messages except in
	 *       the case of inability to transmit.
	 *
	 * @param original The original IPMIMessage transmitted.
	 * @param response The response IPMIMessage received, or nullptr if delivery aborted.
	 */
	typedef std::function<void(std::shared_ptr<IPMIMessage> original, std::shared_ptr<IPMIMessage> response)> response_cb_t;

	/**
	 * Enqueue an outgoing IPMIMessage.
	 *
	 * @note This will update the sequence number on any request (not response) message.
	 *
	 * @param msg  A shared_ptr<IPMIMessage> to enqueue.
	 * @param response_cb  A response callback to be called when a response is
	 *                     received, or on error.  It is never called for a
	 *                     "successful" response message delivery, as these are not
	 *                     ACK'd in IPMI.
	 */
	void send(std::shared_ptr<IPMIMessage> msg, response_cb_t response_cb = nullptr);

	/**
	 * Enqueue an outgoing IPMIMessage.
	 *
	 * @note This will update the sequence number on any request (not response) message.
	 *
	 * @param msg The message to send.
	 * @return If this is a request message, it will wait for and return the
	 *         matching response message.  If the retransmit limit is hit, or an
	 *         error prevents delivery, this will return a nullptr shared_ptr.  If this
	 *         is a response message, it will immediately return a nullptr shared_ptr,
	 *         as there is no acknowledgment mechanism for response messages, so
	 *         there is nothing to block for.
	 */
	std::shared_ptr<IPMIMessage> sendSync(std::shared_ptr<IPMIMessage> msg);

	//! Returns the IPMB address.
	inline uint8_t getIPMBAddress() { return this->kIPMBAddress; };

	// From base class ConsoleCommandSupport:
	virtual void registerConsoleCommands(CommandParser &parser, const std::string &prefix="");
	virtual void deregisterConsoleCommands(CommandParser &parser, const std::string &prefix="");

	/**
	 * TODO
	 */
	typedef struct {
		IPMBSvc *ipmb;
		uint8_t lun;
		uint8_t addr;
	} EventReceiver;

	LogTree &logroot; ///< The root logtree for this object. Not private so IPMI commands can use it.

private:
	// IPMB service commands:
	class SendMsgCommand;
	class EnumerateFruStoragesCommand;
	class DumpFruStorageCommand;
	class StatusCommand;

	/**
	 * A record representing a message in the outgoing message queue.
	 */
	class IPMB_MsgRec {
	public:
		std::shared_ptr<IPMIMessage> msg; ///< The message.
		response_cb_t response_cb;     ///< The response callback, used to report error or success.
		uint8_t retry_count;           ///< The current retry count.
		AbsoluteTimeout next_retry;    ///< The timeout for the next retry.
		IPMB_MsgRec(std::shared_ptr<IPMIMessage> &msg, response_cb_t response_cb = nullptr) : msg(msg), response_cb(response_cb), retry_count(0), next_retry(0ul) { };
	};

	const uint8_t kIPMBAddress;				///< The IPMB address of this node.
	const std::string kName;				///< The name used for this IPMB in StatCounter or Task names, as well as log messages.
	IPMICommandParser *command_parser;		///< The command parser used for incoming messages.
	IPMB &ipmb;								///< The underlying IPMB.
	const size_t recvq_size = 32;			///< The length of the receive queue.
	QueueHandle_t recvq;					///< A queue for received messages from both interfaces.
	SemaphoreHandle_t sendq_mutex;			///< A mutex protecting the sendq.
	SemaphoreHandle_t sendq_notify_sem;		///< A binary semaphore used to signal the task after a .send()
	StatCounter stat_recvq_highwater;		///< A statistics counter for the high-water fill mark for the receive queue.
	StatCounter stat_sendq_highwater;		///< A statistics counter for the high-water fill mark for the transmit queue.
	StatCounter stat_messages_received;		///< How many messages have been received by this IPMB
	StatCounter stat_messages_delivered;	///< How many messages have been delivered successfully by this IPMB
	StatCounter stat_send_attempts;			///< How many delivery attempts have been made by this IPMB
	StatCounter stat_send_failures;			///< How many delivery failures have occurred on this IPMB
	StatCounter stat_no_available_seq;		///< How many times we have run out of sequence numbers for a specific command on this IPMB
	StatCounter stat_unexpected_replies;	///< How many unexpected replies have been received on this IPMB
	LogTree &log_messages_in;				///< Messages received on our IPMB.
	LogTree &log_messages_out;				///< Messages transmitted on our IPMB.

	/**
	 * The number of attempts made to send a given IPMI message.
	 * This must not exceed IPMB spec Table 4-1, "Seq. no. expiration interval" in total duration.
	 */
	const uint8_t kMaxRetries = 10;
	std::list<IPMB_MsgRec> outgoing_messages;	///< The queue of outgoing IPMI messages.
	TaskHandle_t task;							///< A reference to the IPMBSvc task owned by this object.
	QueueSetHandle_t qset;						///< A queueset for use in the thread task.

	PSWDT *wdt;						///< The watchdog.
	PSWDT::slot_handle_t wdt_slot;	///< Our watchdog slot.

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

	/**
	 * This function will assign an available sequence number to an outgoing IPMI message.
	 *
	 * @param msg The IPMIMessage to update the sequence number of.
	 * @return true if success, else false if no valid sequence number was available.
	 */
	bool setSequence(IPMIMessage &msg);

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

	/**
	 * This function will determine whether an incoming IPMI message is a duplicate.
	 *
	 * @param msg The IPMIMessage to check the sequence number of
	 * @return true if duplicate, else false
	 */
	bool checkDuplicate(const IPMIMessage &msg);

	void runThread(); ///< \protected Run the IPMBSvc thread code.
};

#endif /* SRC_COMMON_ZYNQIPMC_SERVICES_IPMI_IPMBSVC_IPMBSVC_H_ */
