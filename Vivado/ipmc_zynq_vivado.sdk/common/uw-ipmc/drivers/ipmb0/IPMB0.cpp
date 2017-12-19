/*
 * IPMB0.cpp
 *
 *  Created on: Dec 8, 2017
 *      Author: jtikalsky
 */

#include <IPMC.h>
#include <drivers/ipmb0/IPMB0.h>
#include "xgpiops.h"
#include <FreeRTOS.h>
#include "queue.h"
#include "task.h"

static void ipmb0_run_thread(void *ipmb_void) {
	reinterpret_cast<IPMB0*>(ipmb_void)->run_thread();
}

/**
 * Instantiate the PS_IPMB.
 *
 * \param ipmbA         The underlying IPMB_A
 * \param ipmbB         The underlying IPMB_B
 * \param ipmb_address  The IPMB address of this node.
 * \param logtree       The logtree for messages from IPMB0.
 */
IPMB0::IPMB0(PS_IPMB *ipmbA, PS_IPMB *ipmbB, uint8_t ipmb_address, LogTree &logtree) :
		ipmb_incoming(SkyRoad::request_messenger<const IPMI_MSG>("ipmb0.incoming_message")),
		ipmb_outgoing(SkyRoad::request_messenger<const IPMI_MSG>("ipmb0.outgoing_message")),
		ipmb_outgoing_failure(SkyRoad::request_messenger<const IPMI_MSG>("ipmb0.outgoing_message_failure")),
		ipmb{ipmbA, ipmbB}, ipmb_address(ipmb_address),
		stat_recvq_highwater("ipmb0.recvq_highwater"),
		stat_sendq_highwater("ipmb0.sendq_highwater"),
		stat_messages_received("ipmb0.messages.received"),
		stat_messages_sent("ipmb0.messages.sent"),
		stat_send_attempts("ipmb0.messages.send_attempts"),
		stat_send_failures("ipmb0.messages.send_failures"),
		stat_no_available_seq("ipmb0.messages.no_available_sequence_number"),
		stat_unexpected_replies("ipmb0.messages.unexpected_replies"),
		log_ipmb0(logtree),
		log_messages_in(logtree["incoming_messages"]),
		log_messages_out(logtree["outgoing_messages"]) {

	configASSERT(ipmbA);
	configASSERT(ipmbB);

	this->recvq = xQueueCreate(this->recvq_size, sizeof(IPMI_MSG));
	configASSERT(this->recvq);

	this->qset = xQueueCreateSet(this->recvq_size + this->temple_size);
	configASSERT(this->qset);

	configASSERT(pdPASS == xQueueAddToSet(this->recvq, this->qset));

	ipmbA->incoming_message_queue = this->recvq;
	ipmbB->incoming_message_queue = this->recvq;

	configASSERT(xTaskCreate(ipmb0_run_thread, "IPMB0", configMINIMAL_STACK_SIZE+256, this, configMAX_PRIORITIES, &this->task));
}

IPMB0::~IPMB0() {
	configASSERT(!this->task); // Clean up the task first.  We can't just TaskDelete as it might be holding a lock at the particular instant.
	this->ipmb[0]->incoming_message_queue = NULL;
	this->ipmb[1]->incoming_message_queue = NULL;
	configASSERT(0); // Destruction is not supported, as QueueSets don't have a good delete functionality.
	vQueueDelete(this->recvq);
}

/**
 * Look up the IPMB address of this node via MIO GPIOs associated with the
 * hardware address assignment pins in on the backplane connector.
 *
 * @param gpios  The MIO pins for the HW address lines.
 * @return       The IPMB address of this node.
 */
uint8_t IPMB0::lookup_ipmb_address(const int gpios[8]) {
	uint8_t address = 0;
	uint8_t parity = 0;
	for (int i = 0; i < 8; ++i) {
		XGpioPs_SetDirectionPin(&gpiops, gpios[i], 0);
		uint8_t val = XGpioPs_ReadPin(&gpiops, gpios[i]);
		configASSERT(val <= 1);
		address |= val << i;
		parity ^= val;
	}
	configASSERT(parity); // Failed address (odd-)parity (bad slot wiring) is simply unsupported at this time.
	return address & 0xfe; // I'm just going to assume this is how it works, given how the IPMB works. TODO: Validate
}

void IPMB0::run_thread() {
	SkyRoad::Temple temple;
	xQueueAddToSet(temple.get_queue(), this->qset);

	AbsoluteTimeout next_wait(UINT64_MAX);
	while (true) {
		// Check for any incoming messages and process them.
		this->stat_recvq_highwater.high_water(uxQueueMessagesWaiting(this->recvq));
		QueueHandle_t q = xQueueSelectFromSet(this->qset, next_wait.get_timeout());
		IPMI_MSG msg;
		if (q == temple.get_queue()) {
			std::shared_ptr<SkyRoad::Envelope> envelope = temple.receive(0);
			configASSERT(envelope); // If it selected it better receive.
			// We only receive one type here, so no Messenger identity check is needed.
			msg = *(envelope->open<const IPMI_MSG>());
			if (this->set_sequence(msg)) {
				this->outgoing_messages.emplace_back(msg);
				this->log_messages_out.log(std::string("Message enqueued for transmit: ") + msg.sprintf(), LogTree::LOG_DIAGNOSTIC);
			}
			else {
				/* We've been flooding this target on this bus with this command
				 * and are now out of unused sequence numbers.  We'll fail this
				 * delivery without even making an attempt.
				 */
				this->stat_no_available_seq.increment();
				this->ipmb_outgoing_failure->send(envelope->open<const IPMI_MSG>()); // We'll just reuse this shared_ptr<const IPMI_MSG>
				this->log_messages_out.log(std::string("Outgoing message discarded, no available sequence number: ") + msg.sprintf(), LogTree::LOG_ERROR);
			}
		}
		else if (q == this->recvq) {
			configASSERT(pdTRUE == xQueueReceive(this->recvq, &msg, 0)); // If it selected it better receive.
			this->stat_messages_received.increment();
			if (msg.netFn & 1) {
				// Pair responses to stop retransmissions.
				bool paired = false;
				for (auto it = this->outgoing_messages.begin(); it != this->outgoing_messages.end(); ) {
					if (it->msg.match_reply(msg)) {
						this->stat_messages_sent.increment();
						paired = true;
						it = this->outgoing_messages.erase(it); // Success!
					}
					else {
						++it;
					}
				}
				if (!paired) {
					this->stat_unexpected_replies.increment();
					this->log_messages_in.log(std::string("Unexpected response received (erroneous retry?): ") + msg.sprintf(), LogTree::LOG_NOTICE);
				}
				else {
					this->log_messages_in.log(std::string("Response received: ") + msg.sprintf(), LogTree::LOG_INFO);
				}
			}
			else {
				this->log_messages_out.log(std::string("Request received:  ") + msg.sprintf(), LogTree::LOG_INFO);
				/* We will tag requests as duplicated, in case this is important
				 * to specific downstream functions, but since IPMI is supposed
				 * to be largely idempotent in terms of handling retransmits,
				 * and they need some kind of response anyway, the message will
				 * still be distributed.
				 */
				msg.duplicate = this->check_duplicate(msg);
			}
			this->ipmb_incoming->send(std::make_shared<const IPMI_MSG>(msg)); // Dispatch a shared_ptr copy.
		}

		// Figure out whether we have any timeouts to wait on next.
		this->stat_sendq_highwater.high_water(this->outgoing_messages.size());
		next_wait.timeout64 = UINT64_MAX;
		for (auto it = this->outgoing_messages.begin(); it != this->outgoing_messages.end(); ) {
			if (it->next_retry.get_timeout() == 0) {
				if (it->retry_count >= this->max_retries) {
					// Delivery failed.  Our last retry timed out.
					this->stat_send_failures.increment();
					this->ipmb_outgoing_failure->send(std::make_shared<const IPMI_MSG>(it->msg));
					this->log_messages_out.log(std::string("Retransmit abandoned: ") + it->msg.sprintf(), LogTree::LOG_WARNING);
					it = this->outgoing_messages.erase(it);
					continue;
				}

				this->stat_send_attempts.increment();
				bool success = this->ipmb[it->retry_count % 2]->send_message(it->msg);
				if (success && (it->msg.netFn & 1) /* response message */) {
					// Sent!  We don't retry responses, so we're done!
					this->stat_messages_sent.increment(); // We won't get a response to pair with this, so increment it now.
					if (it->retry_count == 0)
						this->log_messages_out.log(std::string("Response sent:     ") + it->msg.sprintf(), LogTree::LOG_INFO);
					else
						this->log_messages_out.log(stdsprintf("Response resent:   %s  (retry %hhu)", it->msg.sprintf().c_str(), it->retry_count), LogTree::LOG_NOTICE);
					it = this->outgoing_messages.erase(it);
					continue;
				}
				else {
					if (it->retry_count == 0)
						this->log_messages_out.log(std::string("Request sent:      ") + it->msg.sprintf(), LogTree::LOG_INFO);
					else
						this->log_messages_out.log(stdsprintf("Request resent:    %s  (retry %hhu)", it->msg.sprintf().c_str(), it->retry_count), LogTree::LOG_NOTICE);
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
	}
}

/**
 * This function will assign an available sequence number to an outgoing IPMI message.
 *
 * \param msg The IPMI_MSG to update the sequence number of.
 * \return true if success, else false if no valid sequence number was available.
 */
bool IPMB0::set_sequence(IPMI_MSG &msg) {
	if (msg.netFn & 1)
		return true; // We don't alter the sequence numbers of outgoing replies, that's not our responsibility.

	uint64_t now64 = get_tick64();
	// First, expire old records, for cleanliness.
	for (auto it = this->used_sequence_numbers.begin(); it != this->used_sequence_numbers.end(); ) {
		/* The IPMB spec Table 4-1, specifies the sequence number expiration
		 * interval as 5 seconds.  We'll wait 6 before reuse.
		 */
		if (it->second < now64-(6 * configTICK_RATE_HZ))
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
bool IPMB0::check_duplicate(const IPMI_MSG &msg) {
	uint64_t now64 = get_tick64();

	// First, expire old records, for cleanliness.
	for (auto it = this->incoming_sequence_numbers.begin(); it != this->incoming_sequence_numbers.end(); ) {
		/* The IPMB spec Table 4-1, specifies the sequence number expiration
		 * interval as 5 seconds.
		 */
		if (it->second < now64-(5 * configTICK_RATE_HZ))
			it = this->incoming_sequence_numbers.erase(it);
		else
			++it;
	}

	const uint32_t key = (msg.rqSA << 24) | (msg.netFn << 16) | (msg.cmd << 8) | (msg.rqSeq);
	bool duplicate = this->incoming_sequence_numbers.count(key);
	this->incoming_sequence_numbers[key] = now64;
	return duplicate;
}
