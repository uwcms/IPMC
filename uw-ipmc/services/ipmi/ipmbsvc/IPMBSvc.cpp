/*
 * IPMBSvc.cpp
 *
 *  Created on: Dec 8, 2017
 *      Author: jtikalsky
 */

#include <services/ipmi/ipmbsvc/IPMBSvc.h>
#include <IPMC.h>
#include "xgpiops.h"
#include <FreeRTOS.h>
#include "queue.h"
#include "task.h"

static void ipmb0_run_thread(void *ipmb_void) {
	reinterpret_cast<IPMBSvc*>(ipmb_void)->run_thread();
}

/**
 * Instantiate the IPMB service.
 *
 * \param ipmbA          The underlying IPMB_A
 * \param ipmbB          The underlying IPMB_B (or NULL)
 * \param ipmb_address   The IPMB address of this node.
 * \param command_parser The command parser for incoming commands.
 * \param logtree        The logtree for messages from the IPMBSvc.
 * \param name           Used for StatCounter, Messenger and thread name.
 */
IPMBSvc::IPMBSvc(IPMB *ipmbA, IPMB *ipmbB, uint8_t ipmb_address, IPMICommandParser *command_parser, LogTree &logtree, const std::string name) :
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
		log_messages_out(logtree["outgoing_messages"]) {

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

	configASSERT(xTaskCreate(ipmb0_run_thread, name.c_str(), UWIPMC_STANDARD_STACK_SIZE, this, TASK_PRIORITY_DRIVER, &this->task));
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
	return address & 0xfe; // I'm just going to assume this is how it works, given how the IPMB works. TODO: Validate
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

void IPMBSvc::run_thread() {
	SkyRoad::Temple temple;
	xQueueAddToSet(temple.get_queue(), this->qset);

	AbsoluteTimeout next_wait(UINT64_MAX);
	while (true) {
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
			}
			this->command_parser->dispatch(*this, inmsg);

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
				uint8_t ipmb_choice = it->retry_count % 2;
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
					if (it->retry_count == 0)
						this->log_messages_out.log(std::string("Request sent on ") + this->name + ":      " + it->msg->format(), LogTree::LOG_INFO);
					else
						this->log_messages_out.log(stdsprintf("Request resent on %s:    %s  (retry %hhu)", this->name.c_str(), it->msg->format().c_str(), it->retry_count), LogTree::LOG_NOTICE);
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
