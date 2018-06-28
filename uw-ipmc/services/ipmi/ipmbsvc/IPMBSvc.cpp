/*
 * IPMBSvc.cpp
 *
 *  Created on: Dec 8, 2017
 *      Author: jtikalsky
 */

#include <services/ipmi/ipmbsvc/IPMBSvc.h>
#include <services/console/ConsoleSvc.h>
#include <IPMC.h>
#include "xgpiops.h"
#include <FreeRTOS.h>
#include "queue.h"
#include "task.h"

/**
 * Instantiate the IPMB service.
 *
 * \param ipmbA          The underlying IPMB_A
 * \param ipmbB          The underlying IPMB_B (or NULL)
 * \param ipmb_address   The IPMB address of this node.
 * \param command_parser The command parser for incoming commands.
 * \param logtree        The logtree for messages from the IPMBSvc.
 * \param name           Used for StatCounter, Messenger and thread name.
 * \param wdt            The watchdog instance to register & service.
 */
IPMBSvc::IPMBSvc(IPMB *ipmbA, IPMB *ipmbB, uint8_t ipmb_address, IPMICommandParser *command_parser, LogTree &logtree, const std::string name, PS_WDT *wdt) :
		name(name),
		command_parser(command_parser),
		logroot(logtree),
		ipmb{ipmbA, ipmbB}, ipmb_address(ipmb_address),
		stat_recvq_highwater(name+".recvq_highwater"),
		stat_sendq_highwater(name+".sendq_highwater"),
		stat_messages_received(name+".messages.received"),
		stat_messages_delivered(name+".messages.delivered"),
		stat_send_attempts(name+".messages.send_attempts"),
		stat_send_failures(name+".messages.send_failures"),
		stat_no_available_seq(name+".messages.no_available_sequence_number"),
		stat_unexpected_replies(name+".messages.unexpected_replies"),
		log_messages_in(logtree["incoming_messages"]),
		log_messages_out(logtree["outgoing_messages"]),
		wdt(wdt) {

	configASSERT(ipmbA); // One physical IPMB is required, but not two.

	this->recvq = xQueueCreate(this->recvq_size, sizeof(IPMI_MSG));
	configASSERT(this->recvq);

	this->sendq_notify_sem = xSemaphoreCreateBinary();
	configASSERT(this->sendq_notify_sem);

	this->sendq_mutex = xSemaphoreCreateMutex();
	configASSERT(this->sendq_mutex);

	this->qset = xQueueCreateSet(this->recvq_size + 1);
	configASSERT(this->qset);

	configASSERT(pdPASS == xQueueAddToSet(this->sendq_notify_sem, this->qset));
	configASSERT(pdPASS == xQueueAddToSet(this->recvq, this->qset));

	ipmbA->incoming_message_queue = this->recvq;
	if (ipmbB)
		ipmbB->incoming_message_queue = this->recvq;

	if (this->wdt) {
		this->wdt_slot = this->wdt->register_slot(configTICK_RATE_HZ*10);
		this->wdt->activate_slot(this->wdt_slot);
	}

	this->task = UWTaskCreate(name, TASK_PRIORITY_DRIVER, [this]() -> void { this->run_thread(); });
	configASSERT(this->task);
}

IPMBSvc::~IPMBSvc() {
	configASSERT(!this->task); // Clean up the task first.  We can't just TaskDelete as it might be holding a lock at the particular instant.
	this->ipmb[0]->incoming_message_queue = NULL;
	if (this->ipmb[1])
		this->ipmb[1]->incoming_message_queue = NULL;
	configASSERT(0); // Destruction is not supported, as QueueSets don't have a good delete functionality.
	vSemaphoreDelete(this->sendq_mutex);
	vSemaphoreDelete(this->sendq_notify_sem);
	vQueueDelete(this->recvq);
}

/**
 * Look up the IPMB address of this node via MIO GPIOs associated with the
 * hardware address assignment pins in on the backplane connector.
 *
 * @param gpios  The MIO pins for the HW address lines.
 * @return       The IPMB address of this node.
 */
uint8_t IPMBSvc::lookup_ipmb_address(const int gpios[8]) {
	uint8_t address = 0;
	uint8_t parity = 0;
	for (int i = 0; i < 8; ++i) {
		XGpioPs_SetDirectionPin(&gpiops, gpios[i], 0);
		uint8_t val = XGpioPs_ReadPin(&gpiops, gpios[i]);
		configASSERT(val <= 1);
		address |= val << i;
		parity ^= val;
	}
	/* TODO: Failed address (odd-)parity (bad slot wiring) is simply unsupported
	 * at this time.
	 *
	 * We should eventually find a way to do as specified for this, send an
	 * error report, and at least mention something on the console.
	 */
	configASSERT(parity);
	return (address & 0x7f)<<1; // The high HA bit on the Zone 1 connector is parity.  IPMB addr is HWaddr<<1
}

/**
 * Enqueue an outgoing IPMI_MSG.
 *
 * \note This will update the sequence number on any request (not response) message.
 *
 * @param msg  A shared_ptr<IPMI_MSG> to enqueue.
 * @param response_cb  A response callback to be called when a response is
 *                     received, or on error.  It is never called for a
 *                     "successful" response message delivery, as these are not
 *                     ACK'd in IPMI.
 */
void IPMBSvc::send(std::shared_ptr<IPMI_MSG> msg, response_cb_t response_cb) {
	if (this->set_sequence(*msg)) {
		/* std::list iterators are invalidated only by deleting the item they
		 * point to, so inserts will never affect our iteration.  This allows us
		 * to have this semaphore available and do insertions even during
		 * iteration, such as from a response callback, or when we are
		 * attempting to transmit a message physically onto the bus.  This
		 * should keep wait times here to a minimum.
		 */
		xSemaphoreTake(this->sendq_mutex, portMAX_DELAY);
		this->outgoing_messages.emplace_back(msg, response_cb);
		this->stat_sendq_highwater.high_water(this->outgoing_messages.size());
		xSemaphoreGive(this->sendq_mutex);
		xSemaphoreGive(this->sendq_notify_sem);
		this->log_messages_out.log(std::string("Message enqueued for transmit on ") + this->name + ": " + msg->format(), LogTree::LOG_DIAGNOSTIC);
	}
	else {
		/* We've been flooding this target on this bus with this command
		 * and are now out of unused sequence numbers.  We'll fail this
		 * delivery without even making an attempt.
		 */
		this->stat_no_available_seq.increment();
		if (response_cb)
			response_cb(msg, NULL);
		this->log_messages_out.log(std::string("Outgoing message on ") + this->name + " discarded, no available sequence number: " + msg->format(), LogTree::LOG_ERROR);
	}
}

/**
 * Enqueue an outgoing IPMI_MSG.
 *
 * \note This will update the sequence number on any request (not response) message.
 *
 * \param msg The message to send.
 * \return If this is a request message, it will wait for and return the
 *         matching response message.  If the retransmit limit is hit, or an
 *         error prevents delivery, this will return a NULL shared_ptr.  If this
 *         is a response message, it will immediately return a NULL shared_ptr,
 *         as there is no acknowledgment mechanism for response messages, so
 *         there is nothing to block for.
 */
std::shared_ptr<IPMI_MSG> IPMBSvc::send_sync(std::shared_ptr<IPMI_MSG> msg) {
	if (msg->netFn & 1) {
		// Response message
		this->send(msg, NULL);
		return NULL;
	}
	SemaphoreHandle_t syncsem = xSemaphoreCreateBinary();
	std::shared_ptr<IPMI_MSG> rsp;
	this->send(msg, [&syncsem,&rsp](std::shared_ptr<IPMI_MSG> original, std::shared_ptr<IPMI_MSG> response) -> void {
		rsp = response;
		xSemaphoreGive(syncsem);
	});
	xSemaphoreTake(syncsem, portMAX_DELAY);
	vSemaphoreDelete(syncsem);
	return rsp;
}

void IPMBSvc::run_thread() {
	SkyRoad::Temple temple;
	xQueueAddToSet(temple.get_queue(), this->qset);

	AbsoluteTimeout next_wait(UINT64_MAX);
	while (true) {
		if (this->wdt) {
			uint64_t now64 = get_tick64();
			this->wdt->service_slot(this->wdt_slot);
			if (next_wait.timeout64 > now64 + (configTICK_RATE_HZ/2))
				next_wait.timeout64 = now64 + (configTICK_RATE_HZ/2); // Don't wait past our service frequency.
		}
		// Check for any incoming messages and process them.
		QueueHandle_t q = xQueueSelectFromSet(this->qset, next_wait.get_timeout());
		if (q == this->sendq_notify_sem) {
			configASSERT(pdTRUE == xSemaphoreTake(this->sendq_notify_sem, 0));
			// Notification received.  No specific action to take here.
		}
		else if (q == this->recvq) {
			IPMI_MSG inmsg;
			configASSERT(pdTRUE == xQueueReceive(this->recvq, &inmsg, 0)); // If it selected it better receive.
			UBaseType_t recvq_watermark = uxQueueMessagesWaiting(this->recvq) + 1;
			this->stat_recvq_highwater.high_water(recvq_watermark);
			if (recvq_watermark >= this->recvq_size/2)
				this->log_messages_in.log(stdsprintf("The recvq on %s is %lu%% full with %lu unprocessed messages!", this->name.c_str(), (recvq_watermark*100)/this->recvq_size, recvq_watermark), LogTree::LOG_WARNING);
			this->stat_messages_received.increment();
			if (inmsg.netFn & 1) {
				// Pair responses to stop retransmissions.
				bool paired = false;
				xSemaphoreTake(this->sendq_mutex, portMAX_DELAY);
				for (auto it = this->outgoing_messages.begin(); it != this->outgoing_messages.end(); ++it) {
					if (it->msg->match_reply(inmsg)) {
						paired = true;
						this->stat_messages_delivered.increment();
						this->log_messages_in.log(std::string("Response received on ") + this->name + ": " + inmsg.format(), LogTree::LOG_INFO);
						if (it->response_cb) {
							response_cb_t response_cb = it->response_cb;
							std::shared_ptr<IPMI_MSG> msg = it->msg;
							xSemaphoreGive(this->sendq_mutex);
							response_cb(msg, std::make_shared<IPMI_MSG>(inmsg));
							xSemaphoreTake(this->sendq_mutex, portMAX_DELAY);
						}
						this->outgoing_messages.erase(it); // Success!
						break; // Done.
					}
				}
				xSemaphoreGive(this->sendq_mutex);
				if (!paired) {
					this->stat_unexpected_replies.increment();
					this->log_messages_in.log(std::string("Unexpected response received on ") + this->name + " (erroneous retry?): " + inmsg.format(), LogTree::LOG_NOTICE);
				}
			}
			else {
				/* We will tag requests as duplicated, in case this is important
				 * to specific downstream functions, but since IPMI is supposed
				 * to be largely idempotent in terms of handling retransmits,
				 * and they need some kind of response anyway, the message will
				 * still be distributed.
				 */
				inmsg.duplicate = this->check_duplicate(inmsg);
				if (inmsg.duplicate)
					this->log_messages_in.log(std::string("Request received on ") + this->name + ":  " + inmsg.format() + "  (duplicate)", LogTree::LOG_NOTICE);
				else
					this->log_messages_in.log(std::string("Request received on ") + this->name + ":  " + inmsg.format(), LogTree::LOG_INFO);
				this->command_parser->dispatch(*this, inmsg);
			}

			/* We will attempt to drain our receive queue in preference to
			 * flushing our send queue, as the latter is unbounded and a few
			 * milliseconds of additional transmit or retransmit delay is not
			 * likely to be significant.
			 */
			if (uxQueueMessagesWaiting(this->recvq) > 0) {
				next_wait.timeout64 = 0; // Immediate.
				continue;
			}
		}

		// Figure out whether we have any timeouts to wait on next.
		xSemaphoreTake(this->sendq_mutex, portMAX_DELAY);
		this->stat_sendq_highwater.high_water(this->outgoing_messages.size());
		next_wait.timeout64 = UINT64_MAX;
		for (auto it = this->outgoing_messages.begin(); it != this->outgoing_messages.end(); ) {
			if (it->next_retry.get_timeout() == 0) {
				if (it->retry_count >= this->max_retries) {
					// Delivery failed.  Our last retry timed out.
					this->stat_send_failures.increment();
					this->log_messages_out.log(std::string("Retransmit abandoned on ") + this->name + ": " + it->msg->format(), LogTree::LOG_WARNING);
					if (it->response_cb) {
						response_cb_t response_cb = it->response_cb;
						std::shared_ptr<IPMI_MSG> msg = it->msg;
						xSemaphoreGive(this->sendq_mutex);
						response_cb(msg, NULL);
						xSemaphoreTake(this->sendq_mutex, portMAX_DELAY);
					}
					it = this->outgoing_messages.erase(it);
					continue;
				}

				this->stat_send_attempts.increment();
				uint8_t ipmb_choice = (it->retry_count + it->msg->rqSeq) % 2;
				if (!this->ipmb[1])
					ipmb_choice = 0;

				// We don't want to hold the mutex while waiting on the bus.
				std::shared_ptr<IPMI_MSG> wireout_msg = it->msg;
				xSemaphoreGive(this->sendq_mutex);
				bool success = this->ipmb[ipmb_choice]->send_message(*wireout_msg);
				xSemaphoreTake(this->sendq_mutex, portMAX_DELAY);

				if (success && (it->msg->netFn & 1) /* response message */) {
					// Sent!  We don't retry responses, so we're done!
					this->stat_messages_delivered.increment(); // We won't get a response to pair with this, so increment it now.
					if (it->retry_count == 0)
						this->log_messages_out.log(std::string("Response sent on ") + this->name + ":     " + it->msg->format(), LogTree::LOG_INFO);
					else
						this->log_messages_out.log(stdsprintf("Response resent on %s:   %s  (retry %hhu)", this->name.c_str(), it->msg->format().c_str(), it->retry_count), LogTree::LOG_NOTICE);
					it = this->outgoing_messages.erase(it);
					continue;
				}
				else {
					std::string reqrsp = "Request";
					if (it->msg->netFn & 1)
						reqrsp = "Response";
					std::string result = "";
					if (!success)
						result = " but I2C failed.";
					if (it->retry_count == 0)
						this->log_messages_out.log(reqrsp + " sent on " + this->name + ":      " + it->msg->format() + result, LogTree::LOG_INFO);
					else
						this->log_messages_out.log(stdsprintf("%s resent on %s:    %s  (retry %hhu)%s", reqrsp.c_str(), this->name.c_str(), it->msg->format().c_str(), it->retry_count, result.c_str()), LogTree::LOG_NOTICE);
				}

				/* Now, success or not, we can't discard this yet.
				 *
				 * If we don't get a response, we'll need to retry in...
				 *
				 * Spec: Min 60, Max 250
				 *
				 * Us: 1<<(6+prior_retry_count), cap at 250.
				 */
				TickType_t next_timeout = 1 << (6 + it->retry_count);
				if (next_timeout > 250)
					next_timeout = 250;
				it->next_retry = AbsoluteTimeout(next_timeout);
				it->retry_count++;

				/* We just processed a non-response message.  We'll probably get
				 * a response, and if our out queue is large, might not get to
				 * it in time.  We'll iterate the mainloop now regardless,
				 * therefore.
				 */
				next_wait.timeout64 = 0;
				break;
			}

			if (it->next_retry.timeout64 < next_wait.timeout64)
				next_wait = it->next_retry;
			++it;
		}
		xSemaphoreGive(this->sendq_mutex);
	}
}

/**
 * This function will assign an available sequence number to an outgoing IPMI message.
 *
 * \param msg The IPMI_MSG to update the sequence number of.
 * \return true if success, else false if no valid sequence number was available.
 */
bool IPMBSvc::set_sequence(IPMI_MSG &msg) {
	if (msg.netFn & 1)
		return true; // We don't alter the sequence numbers of outgoing replies, that's not our responsibility.

	uint64_t now64 = get_tick64();
	// First, expire old records, for cleanliness.
	for (auto it = this->used_sequence_numbers.begin(); it != this->used_sequence_numbers.end(); ) {
		/* The IPMB spec Table 4-1, specifies the sequence number expiration
		 * interval as 5 seconds.  We'll wait 6 before reuse, but it has to be
		 * after the last retransmit or it doesn't really make sense.  Let's
		 * estimate.
		 */
		const uint64_t reuse_delay = 6*configTICK_RATE_HZ + 250*this->max_retries;
		if (now64 > reuse_delay && it->second < (now64-reuse_delay))
			it = this->used_sequence_numbers.erase(it);
		else
			++it;
	}

	const uint32_t key = (msg.rsSA << 24) | (msg.netFn << 16) | (msg.cmd << 8);
	for (uint8_t seq = 1; seq < 255; ++seq) {
		if (this->used_sequence_numbers.count(key | seq))
			continue;

		// Sequence number obtained!  Let's register it.
		this->used_sequence_numbers[(key|seq)] = now64;
		msg.rqSeq = seq;
		return true;
	}
	return false; // No valid sequence numbers for this command!  All are used!  (Why the hell are we flooding the bus..?)
}

/**
 * This function will determine whether an incoming IPMI message is a duplicate.
 *
 * \param msg The IPMI_MSG to check the sequence number of
 * \return true if duplicate, else false
 */
bool IPMBSvc::check_duplicate(const IPMI_MSG &msg) {
	uint64_t now64 = get_tick64();

	// First, expire old records, for cleanliness.
	for (auto it = this->incoming_sequence_numbers.begin(); it != this->incoming_sequence_numbers.end(); ) {
		/* The IPMB spec Table 4-1, specifies the sequence number expiration
		 * interval as 5 seconds.
		 */
		if (now64 > 5*configTICK_RATE_HZ && it->second < (now64-(5 * configTICK_RATE_HZ)))
			it = this->incoming_sequence_numbers.erase(it);
		else
			++it;
	}

	const uint32_t key = (msg.rqSA << 24) | (msg.netFn << 16) | (msg.cmd << 8) | (msg.rqSeq);
	bool duplicate = this->incoming_sequence_numbers.count(key);
	this->incoming_sequence_numbers[key] = now64;
	return duplicate;
}


namespace {
/// A "sendmsg" console command.
class ConsoleCommand_sendmsg : public CommandParser::Command {
public:
	IPMBSvc &ipmb; ///< The associated IPMB for this command.

	/// Instantiate
	ConsoleCommand_sendmsg(IPMBSvc &ipmb) : ipmb(ipmb) { };

	virtual std::string get_helptext(const std::string &command) const {
		return stdsprintf(
				"%s $targetaddr $netfn $cmd_hex [$data ...]\n"
				"%s $targetaddr $cmd_name [$data ...]\n"
				"\n"
				"Send an IPMI command on this IPMB.\n"
				"\n"
				"All bytes will be interpreted as hex even without the leading 0x.\n", command.c_str(), command.c_str());
	}

	virtual void execute(std::shared_ptr<ConsoleSvc> console, const CommandParser::CommandParameters &parameters) {
		CommandParser::CommandParameters::xint8_t addr;
		CommandParser::CommandParameters::xint8_t netfn;
		CommandParser::CommandParameters::xint8_t cmd;
		std::vector<std::string>::size_type data_offset = 4;
		if (parameters.nargs() < 3 || !parameters.parse_parameters(1, false, &addr)) {
			print("Invalid parameters.  See help.\n");
			return;
		}
		if (IPMI::cmd_to_id.count(parameters.parameters[2])) {
			// We have a valid string parameter name.  Parse as such.
			--data_offset; // cmd and netfn are not separate parameters here.
			uint16_t ipmicmd = IPMI::cmd_to_id.at(parameters.parameters[2]);
			netfn = ipmicmd >> 8;
			cmd = ipmicmd & 0xff;
		}
		else if (parameters.parse_parameters(2, false, &netfn, &cmd)) {
			// Good.
		}
		else {
			console->write("Invalid parameters.  See help.\n");
			return;
		}

		std::shared_ptr<IPMI_MSG> msg  = std::make_shared<IPMI_MSG>();
		msg->rqSA     = this->ipmb.ipmb_address;
		msg->rqLUN    = 0;
		msg->rsSA     = addr;
		msg->rsLUN    = 0;
		msg->netFn    = netfn;
		msg->cmd      = cmd;
		msg->data_len = 0;
		for (auto i = data_offset; i < parameters.nargs(); ++i) {
			CommandParser::CommandParameters::xint8_t databyte;
			if (!parameters.parse_parameters(i, false, &databyte)) {
				console->write("Invalid IPMI command data.  See help.\n");
				return;
			}
			if (++msg->data_len > msg->max_data_len) {
				console->write("Too much IPMI command data.\n");
				return;
			}
			msg->data[i - data_offset] = databyte;
		}
		this->ipmb.send(msg, [console](std::shared_ptr<IPMI_MSG> original, std::shared_ptr<IPMI_MSG> response) -> void {
			if (response)
				console->write(stdsprintf(
						"Console IPMI command: %s\n"
						"Received response:    %s\n",
						original->format().c_str(), response->format().c_str()));
			else
				console->write(stdsprintf(
						"Console IPMI command: %s\n"
						"Delivery failed.\n",
						original->format().c_str()));
		});
	}

	virtual std::vector<std::string> complete(const CommandParser::CommandParameters &parameters) const {
		if (parameters.cursor_parameter != 2)
			return std::vector<std::string>(); // Sorry, can't help.

		std::vector<std::string> ret;
		for (auto it = IPMI::cmd_to_id.begin(), eit = IPMI::cmd_to_id.end(); it != eit; ++it)
			ret.push_back(it->first);
		return ret;
	};
};

static std::vector<uint8_t> read_fru_area(IPMBSvc &ipmb, uint8_t target, uint8_t frudev, uint16_t offset, uint16_t size, std::function<void(uint16_t offset, uint16_t size)> progress_callback=NULL) {
	std::vector<uint8_t> outbuf;
	outbuf.reserve(size);
	while (size) {
		if (progress_callback)
			progress_callback(offset, size);
		uint8_t rd_size = 0x10;
		if (size < rd_size)
			rd_size = size;

		std::shared_ptr<IPMI_MSG> rd_rsp;
		for (int i = 0; i < 3; ++i) {
			rd_rsp = ipmb.send_sync(
					std::make_shared<IPMI_MSG>(0, ipmb.ipmb_address, 0, target,
							(IPMI::Read_FRU_Data >> 8) & 0xff,
							IPMI::Read_FRU_Data & 0xff,
							std::vector<uint8_t> {
									frudev,
									static_cast<uint8_t>(offset & 0xff),
									static_cast<uint8_t>((offset >> 8) & 0xff),
									rd_size
							}
					)
			);
			if (!rd_rsp || (rd_rsp->data_len && rd_rsp->data[0] == IPMI::Completion::FRU_Device_Busy)) {
				vTaskDelay(333);
				continue;
			}
			break;
		}
		if (!rd_rsp || rd_rsp->data_len < 2 || rd_rsp->data[0] != IPMI::Completion::Success || rd_rsp->data_len != rd_rsp->data[1]+2) {
			outbuf.clear();
			return outbuf;
		}
		outbuf.insert(outbuf.end(), rd_rsp->data + 2, rd_rsp->data + rd_rsp->data_len);
		offset += rd_rsp->data[1];
		size -= rd_rsp->data[1];
	}
	return outbuf;
}

class ConsoleCommand_enumerate_fru_storages : public CommandParser::Command {
public:
	IPMBSvc &ipmb;
	ConsoleCommand_enumerate_fru_storages(IPMBSvc &ipmb) : ipmb(ipmb) { };

	virtual std::string get_helptext(const std::string &command) const {
		return stdsprintf(
					"%s $targetaddr\n"
					"\n"
					"Enumerate all FRU storage devices on a specified device.\n", command.c_str());
	}

	virtual void execute(std::shared_ptr<ConsoleSvc> console, const CommandParser::CommandParameters &parameters) {
		uint8_t ipmbtarget;
		if (!parameters.parse_parameters(1, true, &ipmbtarget)) {
			console->write("Incorrect parameters.  Try help.\n");
			return;
		}
		IPMBSvc *ipmb = &this->ipmb;
		UWTaskCreate("enum_fru_stores", TASK_PRIORITY_INTERACTIVE,
				[ipmb, console, ipmbtarget]() -> void {
					class FRUStorage {
					public:
						uint8_t id;
						uint16_t size;
						bool byte_addressed;
						std::vector<uint8_t> header;
						FRUStorage(uint8_t id, uint16_t size, bool byte_addressed, std::vector<uint8_t> header) : id(id), size(size), byte_addressed(byte_addressed), header(header) { };
					};
					std::vector<FRUStorage> fru_storages;
					for (uint8_t frudev = 0; frudev < 0xff; ++frudev) {
						if ((frudev % 0x10) == 0)
							console->write(stdsprintf("Enumerating FRU Storage Device %02hhXh...\n", frudev));
						std::shared_ptr<IPMI_MSG> probersp;
						for (int retry = 0; retry < 3; ++retry) {
							probersp = ipmb->send_sync(std::make_shared<IPMI_MSG>(
									0, ipmb->ipmb_address,
									0, ipmbtarget,
									(IPMI::Get_FRU_Inventory_Area_Info >> 8) & 0xff, IPMI::Get_FRU_Inventory_Area_Info & 0xff,
									std::vector<uint8_t> {static_cast<uint8_t>(frudev)}));
							if (!probersp || probersp->data_len != 4 || probersp->data[0] != IPMI::Completion::Success) {
								vTaskDelay(333);
								continue;
							}
							break;
						}
						if (probersp && probersp->data_len == 4 && probersp->data[0] == IPMI::Completion::Success) {
							// Okay we probed one up.  Let's read its header.
							std::vector<uint8_t> header = read_fru_area(*ipmb, ipmbtarget, frudev, 0, 8);
							if (header.size()) {
								fru_storages.emplace_back(
										frudev,
										(probersp->data[2]<<8) | probersp->data[1],
										!(probersp->data[3]&1),
										header
										);
							}
							else {
								console->write(stdsprintf("Retries exhausted reading FRU storage header for %02hhXh.\n", frudev));
							}
						}
						else if (probersp && probersp->data_len && probersp->data[0] == IPMI::Completion::Parameter_Out_Of_Range) {
							// No device present.
						}
						else {
							// We ran out of retries.
							if (probersp && probersp->data_len >= 1)
								console->write(stdsprintf("Retries exhausted enumerating FRU storage %02hhXh: Last completion code %02hhXh\n", frudev, probersp->data[0]));
							else
								console->write(stdsprintf("Retries exhausted enumerating FRU storage %02hhXh: No response received.\n", frudev));
						}
					}
					if (fru_storages.empty()) {
						console->write(stdsprintf("No FRU storages found at %02hhXh\n", ipmbtarget));
					}
					else {
						std::string out = "Found FRU Storages:\n";
						for (auto it = fru_storages.begin(), eit = fru_storages.end(); it != eit; ++it) {
							out += stdsprintf("  %02hhXh: %hu %s (Header Version %2hhu)\n", it->id, it->size, it->byte_addressed?"bytes":"words", it->header[0]);
							if (it->header[1])
								out += stdsprintf("       Internal Use Area Offset:  %4hu\n", it->header[1]*8);
							if (it->header[2])
								out += stdsprintf("       Chassis Info Area Offset:  %4hu\n", it->header[2]*8);
							if (it->header[3])
								out += stdsprintf("       Board Area Offset:         %4hu\n", it->header[3]*8);
							if (it->header[4])
								out += stdsprintf("       Product Info Area Offset:  %4hu\n", it->header[4]*8);
							if (it->header[5])
								out += stdsprintf("       Multi-Record  Area Offset: %4hu\n", it->header[5]*8);
						}
						console->write(out);
					}
				});
	}

	//virtual std::vector<std::string> complete(const CommandParser::CommandParameters &parameters) const { };
};

class ConsoleCommand_dump_fru_storage : public CommandParser::Command {
public:
	IPMBSvc &ipmb;
	ConsoleCommand_dump_fru_storage(IPMBSvc &ipmb) : ipmb(ipmb) { };

	virtual std::string get_helptext(const std::string &command) const {
		return stdsprintf(
					"%s $targetaddr $fru_storage_id\n"
					"\n"
					"Dump the contents of the specified FRU storage device.\n", command.c_str());
	}

	virtual void execute(std::shared_ptr<ConsoleSvc> console,
			const CommandParser::CommandParameters &parameters) {
		uint8_t ipmbtarget;
		uint8_t frudev;
		if (!parameters.parse_parameters(1, true, &ipmbtarget, &frudev)) {
			console->write("Incorrect parameters.  Try help.\n");
			return;
		}
		IPMBSvc *ipmb = &this->ipmb;
		UWTaskCreate("dump_fru_store", TASK_PRIORITY_INTERACTIVE,
				[ipmb, console, ipmbtarget, frudev]() -> void {
					std::shared_ptr<IPMI_MSG> probersp = ipmb->send_sync(std::make_shared<IPMI_MSG>(
									0, ipmb->ipmb_address,
									0, ipmbtarget,
									(IPMI::Get_FRU_Inventory_Area_Info >> 8) & 0xff, IPMI::Get_FRU_Inventory_Area_Info & 0xff,
									std::vector<uint8_t> {static_cast<uint8_t>(frudev)}));
					if (!probersp || !probersp->data_len) {
						console->write(stdsprintf("Error querying FRU Inventory Area Info for FRU Device %02hhXh at IPMB address %02hhXh.\n", frudev, ipmbtarget));
						return;
					}
					else if (probersp->data[0] == IPMI::Completion::Parameter_Out_Of_Range) {
						console->write(stdsprintf("No FRU Device %02hhXh found at IPMB address %02hhXh.\n", frudev, ipmbtarget));
						return;
					}
					else if (probersp->data_len != 4 || probersp->data[0] != IPMI::Completion::Success) {
						console->write(stdsprintf("Error querying FRU Inventory Area Info for FRU Device %02hhXh found at IPMB address %02hhXh. (Completion Code %02hhXh)\n", frudev, ipmbtarget, probersp->data[0]));
						return;
					}

					uint16_t ia_size = (probersp->data[2]<<8) | probersp->data[1];
					bool ia_byteaddressed = !(probersp->data[3]&1);
					if (!ia_byteaddressed) {
						console->write("Unable to dump FRU Storage, word access is not supported.\n");
						return;
					}

					std::vector<uint8_t> frubuf = read_fru_area(*ipmb, ipmbtarget, frudev, 0, ia_size,
							[console, ia_size](uint16_t offset, uint16_t size) {
						if ((offset % 0x200) == 0)
							console->write(stdsprintf("Reading FRU Storage... %5hxh/%hxh\n", ia_size-size, ia_size));
					});
					if (frubuf.size()) {
						std::string outbuf = "";
						for (std::vector<uint8_t>::size_type i = 0; i < frubuf.size(); ++i) {
							outbuf += stdsprintf("%02x", frubuf.at(i));
							if (i%16 == 15) {
								outbuf += "\n";
							}
							else if (i%4 == 3) {
								outbuf += "  ";
							}
							else {
								outbuf += " ";
							}
						}
						console->write(outbuf+"\n");
					}
					else {
						console->write("Attempts to read FRU storage failed.\n");
					}
				});
	}

	//virtual std::vector<std::string> complete(const CommandParser::CommandParameters &parameters) const { };
};
} // anonymous namespace

/**
 * Register console commands related to this IPMB.
 * @param parser The command parser to register to.
 * @param prefix A prefix for the registered commands.
 */
void IPMBSvc::register_console_commands(CommandParser &parser, const std::string &prefix) {
	parser.register_command(prefix + "sendmsg", std::make_shared<ConsoleCommand_sendmsg>(*this));
	parser.register_command(prefix + "enumerate_fru_storages", std::make_shared<ConsoleCommand_enumerate_fru_storages>(*this));
	parser.register_command(prefix + "dump_fru_storage", std::make_shared<ConsoleCommand_dump_fru_storage>(*this));
}

/**
 * Unregister console commands related to this IPMB.
 * @param parser The command parser to unregister from.
 * @param prefix A prefix for the registered commands.
 */
void IPMBSvc::deregister_console_commands(CommandParser &parser, const std::string &prefix) {
	parser.register_command(prefix + "sendmsg", NULL);
}
