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
 * \param ipmbA         The underlying IPMB_A
 * \param ipmbB         The underlying IPMB_B (or NULL)
 * \param ipmb_address  The IPMB address of this node.
 * \param logtree       The logtree for messages from the IPMBSvc.
 * \param name          Used for StatCounter, Messenger and thread name.
 */
IPMBSvc::IPMBSvc(IPMB *ipmbA, IPMB *ipmbB, uint8_t ipmb_address, LogTree &logtree, const std::string name) :
		name(name),
		ipmb_incoming(SkyRoad::request_messenger<const IPMI_MSG>(name+".incoming_message")),
		ipmb{ipmbA, ipmbB}, ipmb_address(ipmb_address),
		stat_recvq_highwater(name+".recvq_highwater"),
		stat_sendq_highwater(name+".sendq_highwater"),
		stat_acceptq_highwater(name+".acceptq_highwater"),
		stat_messages_received(name+".messages.received"),
		stat_messages_delivered(name+".messages.delivered"),
		stat_send_attempts(name+".messages.send_attempts"),
		stat_send_failures(name+".messages.send_failures"),
		stat_no_available_seq(name+".messages.no_available_sequence_number"),
		stat_unexpected_replies(name+".messages.unexpected_replies"),
		log_ipmb0(logtree),
		log_messages_in(logtree["incoming_messages"]),
		log_messages_out(logtree["outgoing_messages"]) {

	configASSERT(ipmbA); // One physical IPMB is required, but not two.

	this->recvq = xQueueCreate(this->recvq_size, sizeof(IPMI_MSG));
	configASSERT(this->recvq);

	this->acceptq = xQueueCreate(this->acceptq_size, sizeof(IPMB_MsgRec*));
	configASSERT(this->recvq);

	this->qset = xQueueCreateSet(this->recvq_size + this->acceptq_size);
	configASSERT(this->qset);

	configASSERT(pdPASS == xQueueAddToSet(this->recvq, this->qset));

	ipmbA->incoming_message_queue = this->recvq;
	if (ipmbB)
		ipmbB->incoming_message_queue = this->recvq;

	configASSERT(xTaskCreate(ipmb0_run_thread, name.c_str(), configMINIMAL_STACK_SIZE+512, this, TASK_PRIORITY_DRIVER, &this->task));
}

IPMBSvc::~IPMBSvc() {
	configASSERT(!this->task); // Clean up the task first.  We can't just TaskDelete as it might be holding a lock at the particular instant.
	this->ipmb[0]->incoming_message_queue = NULL;
	if (this->ipmb[1])
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
	configASSERT(parity); // Failed address (odd-)parity (bad slot wiring) is simply unsupported at this time.
	return address & 0xfe; // I'm just going to assume this is how it works, given how the IPMB works. TODO: Validate
}

/**
 * Enqueue an outgoing IPMI_MSG.
 *
 * \note Once a request (not response) message has been accepted by the IPMBSvc
 *       task for delivery, the sequence number will be updated.
 *
 * @param msg  A shared_ptr<IPMI_MSG> to enqueue.
 * @param response_cb  A response callback to be called when a response is
 *                     received, or on error.  It is never called for a
 *                     "successful" response message delivery, as these are not
 *                     ACK'd in IPMI.
 */
void IPMBSvc::send(std::shared_ptr<IPMI_MSG> msg, response_cb_t response_cb) {
	/* This technique is more elastic than using a mutex protecting the
	 * outgoing message queue and directly inserting records, as that mutex
	 * would have to be held for the entire iteration of the list when
	 * transmitting enqueued outgoing messages.  This way there is some buffer
	 * before tasks block, rather than forcing them to block immediately for
	 * possibly that full duration.
	 */
	IPMB_MsgRec *acceptmsg = new IPMB_MsgRec(msg, response_cb);
	xQueueSend(this->acceptq, &acceptmsg, portMAX_DELAY);
}

void IPMBSvc::run_thread() {
	SkyRoad::Temple temple;
	xQueueAddToSet(temple.get_queue(), this->qset);

	AbsoluteTimeout next_wait(UINT64_MAX);
	while (true) {
		// Check for any incoming messages and process them.
		QueueHandle_t q = xQueueSelectFromSet(this->qset, next_wait.get_timeout());
		if (q == this->acceptq) {
			IPMB_MsgRec *acceptmsg;
			configASSERT(pdTRUE == xQueueReceive(this->acceptq, &acceptmsg, 0)); // If it selected it better receive.
			UBaseType_t acceptq_watermark = uxQueueMessagesWaiting(this->acceptq) + 1;
			this->stat_acceptq_highwater.high_water(acceptq_watermark);
			if (acceptq_watermark >= this->acceptq_size/2)
				this->log_messages_in.log(stdsprintf("The IPMBSvc acceptq is %lu%% full with %lu unprocessed messages!", (acceptq_watermark*100)/this->acceptq_size, acceptq_watermark), LogTree::LOG_WARNING);
			if (this->set_sequence(*acceptmsg->msg)) {
				this->outgoing_messages.push_back(*acceptmsg);
				this->log_messages_out.log(std::string("Message enqueued for transmit: ") + acceptmsg->msg->format(), LogTree::LOG_DIAGNOSTIC);
			}
			else {
				/* We've been flooding this target on this bus with this command
				 * and are now out of unused sequence numbers.  We'll fail this
				 * delivery without even making an attempt.
				 */
				this->stat_no_available_seq.increment();
				if (acceptmsg->response_cb)
					acceptmsg->response_cb(acceptmsg->msg, NULL);
				this->log_messages_out.log(std::string("Outgoing message discarded, no available sequence number: ") + acceptmsg->msg->format(), LogTree::LOG_ERROR);
			}
			delete acceptmsg;
		}
		else if (q == this->recvq) {
			IPMI_MSG inmsg;
			configASSERT(pdTRUE == xQueueReceive(this->recvq, &inmsg, 0)); // If it selected it better receive.
			UBaseType_t recvq_watermark = uxQueueMessagesWaiting(this->recvq) + 1;
			this->stat_recvq_highwater.high_water(recvq_watermark);
			if (recvq_watermark >= this->recvq_size/2)
				this->log_messages_in.log(stdsprintf("The IPMBSvc recvq is %lu%% full with %lu unprocessed messages!", (recvq_watermark*100)/this->recvq_size, recvq_watermark), LogTree::LOG_WARNING);
			this->stat_messages_received.increment();
			if (inmsg.netFn & 1) {
				// Pair responses to stop retransmissions.
				bool paired = false;
				for (auto it = this->outgoing_messages.begin(); it != this->outgoing_messages.end(); ++it) {
					if (it->msg->match_reply(inmsg)) {
						paired = true;
						this->stat_messages_delivered.increment();
						this->log_messages_in.log(std::string("Response received: ") + inmsg.format(), LogTree::LOG_INFO);
						if (it->response_cb)
							it->response_cb(it->msg, std::make_shared<IPMI_MSG>(inmsg));
						this->outgoing_messages.erase(it); // Success!
						break; // Done.
					}
				}
				if (!paired) {
					this->stat_unexpected_replies.increment();
					this->log_messages_in.log(std::string("Unexpected response received (erroneous retry?): ") + inmsg.format(), LogTree::LOG_NOTICE);
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
					this->log_messages_in.log(std::string("Request received:  ") + inmsg.format() + "  (duplicate)", LogTree::LOG_NOTICE);
				else
					this->log_messages_in.log(std::string("Request received:  ") + inmsg.format(), LogTree::LOG_INFO);
			}
			this->ipmb_incoming->send(std::make_shared<const IPMI_MSG>(inmsg)); // Dispatch a shared_ptr copy.
		}

		// Figure out whether we have any timeouts to wait on next.
		this->stat_sendq_highwater.high_water(this->outgoing_messages.size());
		next_wait.timeout64 = UINT64_MAX;
		for (auto it = this->outgoing_messages.begin(); it != this->outgoing_messages.end(); ) {
			if (it->next_retry.get_timeout() == 0) {
				if (it->retry_count >= this->max_retries) {
					// Delivery failed.  Our last retry timed out.
					this->stat_send_failures.increment();
					this->log_messages_out.log(std::string("Retransmit abandoned: ") + it->msg->format(), LogTree::LOG_WARNING);
					if (it->response_cb)
						it->response_cb(it->msg, NULL);
					it = this->outgoing_messages.erase(it);
					continue;
				}

				this->stat_send_attempts.increment();
				uint8_t ipmb_choice = it->retry_count % 2;
				if (!this->ipmb[1])
					ipmb_choice = 0;
				bool success = this->ipmb[ipmb_choice]->send_message(*it->msg);
				if (success && (it->msg->netFn & 1) /* response message */) {
					// Sent!  We don't retry responses, so we're done!
					this->stat_messages_delivered.increment(); // We won't get a response to pair with this, so increment it now.
					if (it->retry_count == 0)
						this->log_messages_out.log(std::string("Response sent:     ") + it->msg->format(), LogTree::LOG_INFO);
					else
						this->log_messages_out.log(stdsprintf("Response resent:   %s  (retry %hhu)", it->msg->format().c_str(), it->retry_count), LogTree::LOG_NOTICE);
					it = this->outgoing_messages.erase(it);
					continue;
				}
				else {
					if (it->retry_count == 0)
						this->log_messages_out.log(std::string("Request sent:      ") + it->msg->format(), LogTree::LOG_INFO);
					else
						this->log_messages_out.log(stdsprintf("Request resent:    %s  (retry %hhu)", it->msg->format().c_str(), it->retry_count), LogTree::LOG_NOTICE);
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
bool IPMBSvc::set_sequence(IPMI_MSG &msg) {
	if (msg.netFn & 1)
		return true; // We don't alter the sequence numbers of outgoing replies, that's not our responsibility.

	uint64_t now64 = get_tick64();
	// First, expire old records, for cleanliness.
	for (auto it = this->used_sequence_numbers.begin(); it != this->used_sequence_numbers.end(); ) {
		/* The IPMB spec Table 4-1, specifies the sequence number expiration
		 * interval as 5 seconds.  We'll wait 6 before reuse.
		 */
		if (now64 > 6*configTICK_RATE_HZ && it->second < (now64-(6 * configTICK_RATE_HZ)))
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
