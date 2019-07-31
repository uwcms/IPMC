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

#include <core.h>
#include "xgpiops.h"
#include <FreeRTOS.h>
#include "queue.h"
#include "task.h"
#include <time.h>
#include <libs/printf.h>
#include <libs/except.h>
#include <services/console/consolesvc.h>
#include <services/ipmi/ipmbsvc/ipmbsvc.h>
#include <services/ipmi/remote_fru_storage.h>

IPMBSvc::IPMBSvc(IPMB &ipmb, uint8_t ipmb_address, IPMICommandParser *command_parser, LogTree &logtree, const std::string name, PSWDT *wdt, bool wait_for_service_init) :
		logroot(logtree),
		kIPMBAddress(ipmb_address),
		kName(name),
		command_parser(command_parser),
		ipmb(ipmb),
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
	this->recvq = xQueueCreate(this->recvq_size, sizeof(IPMIMessage));
	configASSERT(this->recvq);

	this->sendq_notify_sem = xSemaphoreCreateBinary();
	configASSERT(this->sendq_notify_sem);

	this->sendq_mutex = xSemaphoreCreateMutex();
	configASSERT(this->sendq_mutex);

	this->qset = xQueueCreateSet(this->recvq_size + 1);
	configASSERT(this->qset);

	configASSERT(pdPASS == xQueueAddToSet(this->sendq_notify_sem, this->qset));
	configASSERT(pdPASS == xQueueAddToSet(this->recvq, this->qset));

	ipmb.setIncomingMessageQueue(this->recvq);

	if (this->wdt) {
		this->wdt_slot = this->wdt->register_slot(configTICK_RATE_HZ*10);
		this->wdt->activate_slot(this->wdt_slot);
	}

	this->task = runTask(name, TASK_PRIORITY_DRIVER, [this, wait_for_service_init]() -> void {
		if (wait_for_service_init)
			xEventGroupWaitBits(init_complete, 0x3, pdFALSE, pdTRUE, portMAX_DELAY);
		this->runThread();
	});
}

IPMBSvc::~IPMBSvc() {
	configASSERT(!this->task); // Clean up the task first.  We can't just TaskDelete as it might be holding a lock at the particular instant.
	this->ipmb.setIncomingMessageQueue(nullptr);
	configASSERT(0); // Destruction is not supported, as QueueSets don't have a good delete functionality.
	vSemaphoreDelete(this->sendq_mutex);
	vSemaphoreDelete(this->sendq_notify_sem);
	vQueueDelete(this->recvq);
}

void IPMBSvc::send(std::shared_ptr<IPMIMessage> msg, response_cb_t response_cb) {
	if (this->setSequence(*msg)) {
		/* std::list iterators are invalidated only by deleting the item they
		 * point to, so inserts will never affect our iteration.  This allows us
		 * to have this semaphore available and do insertions even during
		 * iteration, such as from a response callback, or when we are
		 * attempting to transmit a message physically onto the bus.  This
		 * should keep wait times here to a minimum.
		 */
		MutexGuard<false> lock(this->sendq_mutex, true);
		this->outgoing_messages.emplace_back(msg, response_cb);
		this->stat_sendq_highwater.highWater(this->outgoing_messages.size());
		lock.release();
		xSemaphoreGive(this->sendq_notify_sem);
		this->log_messages_out.log(std::string("Message enqueued for transmit on ") + this->kName + ": " + msg->format(), LogTree::LOG_DIAGNOSTIC);
	} else {
		/* We've been flooding this target on this bus with this command
		 * and are now out of unused sequence numbers.  We'll fail this
		 * delivery without even making an attempt.
		 */
		this->stat_no_available_seq.increment();
		if (response_cb)
			response_cb(msg, nullptr);
		this->log_messages_out.log(std::string("Outgoing message on ") + this->kName + " discarded, no available sequence number: " + msg->format(), LogTree::LOG_ERROR);
	}
}

std::shared_ptr<IPMIMessage> IPMBSvc::sendSync(std::shared_ptr<IPMIMessage> msg) {
	if (msg->netFn & 1) {
		// Response message
		this->send(msg, nullptr);
		return nullptr;
	}

	SemaphoreHandle_t syncsem = xSemaphoreCreateBinary();
	std::shared_ptr<IPMIMessage> rsp;

	this->send(msg, [&syncsem,&rsp](std::shared_ptr<IPMIMessage> original, std::shared_ptr<IPMIMessage> response) -> void {
		rsp = response;
		xSemaphoreGive(syncsem);
	});

	xSemaphoreTake(syncsem, portMAX_DELAY);
	vSemaphoreDelete(syncsem);
	return rsp;
}

void IPMBSvc::runThread() {
	AbsoluteTimeout next_wait(UINT64_MAX);
	while (true) {
		if (this->wdt) {
			uint64_t now64 = get_tick64();
			this->wdt->service_slot(this->wdt_slot);
			if (next_wait.getTimeout64() > (now64 + (configTICK_RATE_HZ/2)))
				next_wait.setAbsTimeout(now64 + (configTICK_RATE_HZ/2)); // Don't wait past our service frequency.
		}

		// Check for any incoming messages and process them.
		QueueHandle_t q = xQueueSelectFromSet(this->qset, next_wait.getTimeout());
		if (q == this->sendq_notify_sem) {
			configASSERT(pdTRUE == xSemaphoreTake(this->sendq_notify_sem, 0));
			// Notification received.  No specific action to take here.
		} else if (q == this->recvq) {
			IPMIMessage inmsg;
			configASSERT(pdTRUE == xQueueReceive(this->recvq, &inmsg, 0)); // If it selected it better receive.
			UBaseType_t recvq_watermark = uxQueueMessagesWaiting(this->recvq) + 1;
			this->stat_recvq_highwater.highWater(recvq_watermark);
			if (recvq_watermark >= this->recvq_size/2)
				this->log_messages_in.log(stdsprintf("The recvq on %s is %lu%% full with %lu unprocessed messages!", this->kName.c_str(), (recvq_watermark*100)/this->recvq_size, recvq_watermark), LogTree::LOG_WARNING);
			this->stat_messages_received.increment();
			if (inmsg.netFn & 1) {
				// Pair responses to stop retransmissions.
				bool paired = false;
				MutexGuard<false> lock(this->sendq_mutex, true);
				for (auto it = this->outgoing_messages.begin(); it != this->outgoing_messages.end(); ++it) {
					if (it->msg->matchReply(inmsg)) {
						paired = true;
						this->stat_messages_delivered.increment();
						this->log_messages_in.log(std::string("Response received on ") + this->kName + ": " + inmsg.format(), LogTree::LOG_INFO);
						if (it->response_cb) {
							response_cb_t response_cb = it->response_cb;
							std::shared_ptr<IPMIMessage> msg = it->msg;
							lock.release();
							response_cb(msg, std::make_shared<IPMIMessage>(inmsg));
							lock.acquire();
						}
						this->outgoing_messages.erase(it); // Success!
						break; // Done.
					}
				}
				lock.release();
				if (!paired) {
					this->stat_unexpected_replies.increment();
					this->log_messages_in.log(std::string("Unexpected response received on ") + this->kName + " (erroneous retry?): " + inmsg.format(), LogTree::LOG_NOTICE);
				}
			} else {
				/* We will tag requests as duplicated, in case this is important
				 * to specific downstream functions, but since IPMI is supposed
				 * to be largely idempotent in terms of handling retransmits,
				 * and they need some kind of response anyway, the message will
				 * still be distributed.
				 */
				inmsg.duplicate = this->checkDuplicate(inmsg);
				if (inmsg.duplicate) {
					this->log_messages_in.log(std::string("Request received on ") + this->kName + ":  " + inmsg.format() + "  (duplicate)", LogTree::LOG_NOTICE);
				} else {
					this->log_messages_in.log(std::string("Request received on ") + this->kName + ":  " + inmsg.format(), LogTree::LOG_INFO);
				}
				this->command_parser->dispatch(*this, inmsg);
			}

			/* We will attempt to drain our receive queue in preference to
			 * flushing our send queue, as the latter is unbounded and a few
			 * milliseconds of additional transmit or retransmit delay is not
			 * likely to be significant.
			 */
			if (uxQueueMessagesWaiting(this->recvq) > 0) {
				next_wait.setAbsTimeout(0); // Immediate.
				continue;
			}
		}

		// Figure out whether we have any timeouts to wait on next.
		MutexGuard<false> lock(this->sendq_mutex, true);
		this->stat_sendq_highwater.highWater(this->outgoing_messages.size());
		next_wait.setAbsTimeout(UINT64_MAX);
		for (auto it = this->outgoing_messages.begin(); it != this->outgoing_messages.end(); ) {
			if (it->next_retry.getTimeout() == 0) {
				if (it->retry_count >= this->kMaxRetries) {
					// Delivery failed.  Our last retry timed out.
					this->stat_send_failures.increment();
					this->log_messages_out.log(std::string("Retransmit abandoned on ") + this->kName + ": " + it->msg->format(), LogTree::LOG_WARNING);
					if (it->response_cb) {
						response_cb_t response_cb = it->response_cb;
						std::shared_ptr<IPMIMessage> msg = it->msg;
						lock.release();
						response_cb(msg, nullptr);
						lock.acquire();
					}
					it = this->outgoing_messages.erase(it);
					continue;
				}

				this->stat_send_attempts.increment();

				// We don't want to hold the mutex while waiting on the bus.
				std::shared_ptr<IPMIMessage> wireout_msg = it->msg;
				lock.release();
				bool success;
				if (wireout_msg->rsSA == this->kIPMBAddress && wireout_msg->rsLUN == 0) {
					// TODO: Support an understanding of what LUN is local to us. (Also in general rqLUN.)
					/* This is a loopback message, destined for us.
					 * Deliver it to our incoming queue.
					 *
					 * We can't listen on the bus, and send to it, at the same time.
					 */
					success = (pdTRUE == xQueueSend(this->recvq, &*wireout_msg, 0));
				} else {
					// This is a normal outgoing message, deliver it.
					success = this->ipmb.sendMessage(*wireout_msg, it->retry_count);
				}
				lock.acquire();

				if (success && (it->msg->netFn & 1) /* response message */) {
					// Sent!  We don't retry responses, so we're done!
					this->stat_messages_delivered.increment(); // We won't get a response to pair with this, so increment it now.
					if (it->retry_count == 0) {
						this->log_messages_out.log(std::string("Response sent on ") + this->kName + ":     " + it->msg->format(), LogTree::LOG_INFO);
					} else {
						this->log_messages_out.log(stdsprintf("Response resent on %s:   %s  (retry %hhu)", this->kName.c_str(), it->msg->format().c_str(), it->retry_count), (it->retry_count <= 2 ? LogTree::LOG_INFO : LogTree::LOG_NOTICE));
					}
					it = this->outgoing_messages.erase(it);
					continue;
				} else {
					std::string reqrsp = "Request";
					if (it->msg->netFn & 1)
						reqrsp = "Response";
					std::string result = "";
					if (!success)
						result = " but I2C failed.";
					if (it->retry_count == 0) {
						this->log_messages_out.log(reqrsp + " sent on " + this->kName + ":      " + it->msg->format() + result, LogTree::LOG_INFO);
					} else {
						this->log_messages_out.log(stdsprintf("%s resent on %s:    %s  (retry %hhu)%s", reqrsp.c_str(), this->kName.c_str(), it->msg->format().c_str(), it->retry_count, result.c_str()), LogTree::LOG_NOTICE);
					}
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
				next_wait.setAbsTimeout(0);
				break;
			}

			if (it->next_retry.getTimeout64() < next_wait.getTimeout64())
				next_wait = it->next_retry;
			++it;
		}
	}
}

bool IPMBSvc::setSequence(IPMIMessage &msg) {
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
		const uint64_t reuse_delay = 6*configTICK_RATE_HZ + 250*this->kMaxRetries;
		if (now64 > reuse_delay && it->second < (now64-reuse_delay)) {
			it = this->used_sequence_numbers.erase(it);
		} else {
			++it;
		}
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

bool IPMBSvc::checkDuplicate(const IPMIMessage &msg) {
	uint64_t now64 = get_tick64();

	// First, expire old records, for cleanliness.
	for (auto it = this->incoming_sequence_numbers.begin(); it != this->incoming_sequence_numbers.end(); ) {
		/* The IPMB spec Table 4-1, specifies the sequence number expiration
		 * interval as 5 seconds.
		 */
		if (now64 > 5*configTICK_RATE_HZ && it->second < (now64-(5 * configTICK_RATE_HZ))) {
			it = this->incoming_sequence_numbers.erase(it);
		} else {
			++it;
		}
	}

	const uint32_t key = (msg.rqSA << 24) | (msg.netFn << 16) | (msg.cmd << 8) | (msg.rqSeq);
	bool duplicate = this->incoming_sequence_numbers.count(key);
	this->incoming_sequence_numbers[key] = now64;
	return duplicate;
}


//! A "sendmsg" console command.
class IPMBSvc::SendMsgCommand : public CommandParser::Command {
public:
	SendMsgCommand(IPMBSvc &ipmb) : ipmb(ipmb) { };

	virtual std::string get_helptext(const std::string &command) const {
		return command + " $targetaddr $netfn $cmd_hex [$data ...]\n" +
				command + " $targetaddr $cmd_name [$data ...]\n\n"
				"Send an IPMI command on this IPMB.\n"
				"\n"
				"All bytes will be interpreted as hex even without the leading 0x.\n";
	}

	virtual void execute(std::shared_ptr<ConsoleSvc> console, const CommandParser::CommandParameters &parameters) {
		CommandParser::CommandParameters::xint8_t addr;
		CommandParser::CommandParameters::xint8_t netfn;
		CommandParser::CommandParameters::xint8_t cmd;
		std::vector<std::string>::size_type data_offset = 4;

		if (parameters.nargs() < 3 || !parameters.parseParameters(1, false, &addr)) {
			print("Invalid parameters.  See help.\n");
			return;
		}

		if (IPMI::cmd_to_id.count(parameters.parameters[2])) {
			// We have a valid string parameter name.  Parse as such.
			--data_offset; // cmd and netfn are not separate parameters here.
			uint16_t ipmicmd = IPMI::cmd_to_id.at(parameters.parameters[2]);
			netfn = ipmicmd >> 8;
			cmd = ipmicmd & 0xff;
		} else if (parameters.parseParameters(2, false, &netfn, &cmd)) {
			// Good.
		} else {
			console->write("Invalid parameters.  See help.\n");
			return;
		}

		std::shared_ptr<IPMIMessage> msg  = std::make_shared<IPMIMessage>();
		msg->rqSA     = this->ipmb.kIPMBAddress;
		msg->rqLUN    = 0;
		msg->rsSA     = addr;
		msg->rsLUN    = 0;
		msg->netFn    = netfn;
		msg->cmd      = cmd;
		msg->data_len = 0;

		for (auto i = data_offset; i < parameters.nargs(); ++i) {
			CommandParser::CommandParameters::xint8_t databyte;
			if (!parameters.parseParameters(i, false, &databyte)) {
				console->write("Invalid IPMI command data.  See help.\n");
				return;
			}

			if (++msg->data_len > msg->max_data_len) {
				console->write("Too much IPMI command data.\n");
				return;
			}

			msg->data[i - data_offset] = databyte;
		}
		this->ipmb.send(msg, [console](std::shared_ptr<IPMIMessage> original, std::shared_ptr<IPMIMessage> response) -> void {
			if (response) {
				console->write(stdsprintf(
						"Console IPMI command: %s\n"
						"Received response:    %s\n",
						original->format().c_str(), response->format().c_str()));
			} else {
				console->write(stdsprintf(
						"Console IPMI command: %s\n"
						"Delivery failed.\n",
						original->format().c_str()));
			}
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

private:
	IPMBSvc &ipmb;
};

//! A "enumerate_fru_storages" console command.
class IPMBSvc::EnumerateFruStoragesCommand : public CommandParser::Command {
public:
	EnumerateFruStoragesCommand(IPMBSvc &ipmb) : ipmb(ipmb) { };

	virtual std::string get_helptext(const std::string &command) const {
		return command + " $targetaddr\n\n"
				"Enumerate all FRU storage devices on a specified device.\n";
	}

	virtual void execute(std::shared_ptr<ConsoleSvc> console, const CommandParser::CommandParameters &parameters) {
		uint8_t ipmbtarget;
		IPMBSvc *ipmb = &this->ipmb;

		if (!parameters.parseParameters(1, true, &ipmbtarget)) {
			console->write("Incorrect parameters.  Try help.\n");
			return;
		}

		runTask("enum_fru_stores", TASK_PRIORITY_INTERACTIVE, [ipmb, console, ipmbtarget]() -> void {
			std::vector<RemoteFRUStorage> fru_storages;
			for (uint8_t frudev = 0; frudev < 0xff; ++frudev) {
				if ((frudev % 0x10) == 0)
					console->write(stdsprintf("Enumerating FRU Storage Device %02hhXh...\n", frudev));

				std::shared_ptr<RemoteFRUStorage> storage = RemoteFRUStorage::probe(ipmb, ipmbtarget, frudev);
				if (storage) {
					// Okay we probed one up.  Let's read its header.
					if (!storage->readHeader()) {
						console->write(stdsprintf("Unable to read FRU storage header for %02hhXh.\n", frudev));
						continue;
					}

					fru_storages.push_back(*storage);
				}
			}

			if (fru_storages.empty()) {
				console->write(stdsprintf("No FRU storages found at %02hhXh\n", ipmbtarget));
			} else {
				std::string out = "Found FRU Storages:\n";
				for (auto it = fru_storages.begin(), eit = fru_storages.end(); it != eit; ++it) {
					out += stdsprintf("  %02hhXh: %hu %s (Header Version %2hhu)\n", it->getFruDeviceId(), it->getStorageAreaSize(), it->isByteAddressed()?"bytes":"words", it->getHeaderVersion());
					if (it->getInternalUseAreaOffset())
						out += stdsprintf("       Internal Use Area Offset:  %4hu\n", it->getInternalUseAreaOffset());
					if (it->getChassisInfoAreaOffset())
						out += stdsprintf("       Chassis Info Area Offset:  %4hu\n", it->getChassisInfoAreaOffset());
					if (it->getBoardAreaOffset())
						out += stdsprintf("       Board Area Offset:         %4hu\n", it->getBoardAreaOffset());
					if (it->getProductInfoAreaOffset())
						out += stdsprintf("       Product Info Area Offset:  %4hu\n", it->getProductInfoAreaOffset());
					if (it->getMultirecordAreaOffset())
						out += stdsprintf("       Multi-Record  Area Offset: %4hu\n", it->getMultirecordAreaOffset());
				}
				console->write(out);
			}
		});
	}

private:
	IPMBSvc &ipmb;
};

//! A "dump_fru_storage" console command.
class IPMBSvc::DumpFruStorageCommand : public CommandParser::Command {
public:
	DumpFruStorageCommand(IPMBSvc &ipmb) : ipmb(ipmb) { };

	virtual std::string get_helptext(const std::string &command) const {
		return command + " $targetaddr $fru_storage_id\n\n"
				"Dump the contents of the specified FRU storage device.\n";
	}

	virtual void execute(std::shared_ptr<ConsoleSvc> console, const CommandParser::CommandParameters &parameters) {
		uint8_t ipmbtarget;
		uint8_t frudev;
		bool all = false;

		if (!parameters.parseParameters(1, true, &ipmbtarget, &frudev, &all)) {
			if (!parameters.parseParameters(1, true, &ipmbtarget, &frudev)) {
				console->write("Incorrect parameters.  Try help.\n");
				return;
			}
		}
		IPMBSvc *ipmb = &this->ipmb;

		runTask("dump_fru_store", TASK_PRIORITY_INTERACTIVE, [ipmb, console, ipmbtarget, frudev, all]() -> void {
			std::shared_ptr<RemoteFRUStorage> storage = RemoteFRUStorage::probe(ipmb, ipmbtarget, frudev);
			if (!storage) {
				console->write(stdsprintf("Error querying FRU Inventory Area Info for FRU Device %02hhXh at IPMB address %02hhXh.\n", frudev, ipmbtarget));
				return;
			}

			if (!storage->isByteAddressed()) {
				console->write("Unable to dump FRU Storage, word access is not supported.\n");
				return;
			}

			if (storage->getInternalUseAreaOffset()) {
				console->write("Internal Use Area: Present\n");
			}

			if (storage->isHeaderValid()) {
				if (storage->getChassisInfoAreaOffset()) {
					std::shared_ptr<RemoteFRUStorage::ChassisInfo> chassis = storage->readChassisInfoArea();
					std::string outbuf = "Chassis Information Area:\n";
					if (!chassis) {
						outbuf += "  Unreadable.\n";
					} else {
						outbuf += stdsprintf("  Info Area Version:  %hhu\n", chassis->info_area_version);
						if (RemoteFRUStorage::ChassisInfo::chassis_type_descriptions.count(chassis->type)) {
							outbuf += stdsprintf("  Chassis Type:       0x%02hhx \"%s\"\n", chassis->type, RemoteFRUStorage::ChassisInfo::chassis_type_descriptions.at(chassis->type).c_str());
						} else {
							outbuf += stdsprintf("  Chassis Type:       0x%02hhx\n", chassis->type);
						}

						outbuf += stdsprintf("  Part Number:        \"%s\"\n", chassis->part_number.c_str());
						outbuf += stdsprintf("  Serial Number:      \"%s\"\n", chassis->serial_number.c_str());

						if (chassis->custom_info.size()) {
							outbuf += "  Custom Info:\n";
							for (auto it = chassis->custom_info.begin(), eit = chassis->custom_info.end(); it != eit; ++it) {
								outbuf += std::string("    \"") + *it + "\"\n";
							}
						}
					}
					console->write(outbuf);
				}

				if (storage->getBoardAreaOffset()) {
					std::shared_ptr<RemoteFRUStorage::BoardArea> board = storage->readBoardArea();
					std::string outbuf = "Board Area:\n";
					if (!board) {
						outbuf += "  Unreadable.\n";
					} else {
						outbuf += stdsprintf("  Board Area Version: %hhu\n", board->board_area_version);
						if (RemoteFRUStorage::language_codes.count(board->language_code)) {
							outbuf += stdsprintf("  Language Code:      0x%02hhx \"%s\"\n", board->language_code, RemoteFRUStorage::language_codes.at(board->language_code).c_str());
						} else {
							outbuf += stdsprintf("  Language Code:      0x%02hhx\n", board->language_code);
						}

						if (board->mfg_timestamp) {
							time_t mfgt = board->mfg_timestamp;
							struct tm mfgtm;
							gmtime_r(&mfgt, &mfgtm);
							char mfgts[24];
							strftime(mfgts, 24, "%Y-%m-%d %H:%M:%S", &mfgtm);
							outbuf += stdsprintf("  Mfg. Date:          %s\n", mfgts);
						} else {
							outbuf += "  Mfg. Date:          Unspecified\n";
						}

						outbuf += stdsprintf("  Manufacturer:       \"%s\"\n", board->manufacturer.c_str());
						outbuf += stdsprintf("  Product Name:       \"%s\"\n", board->product_name.c_str());
						outbuf += stdsprintf("  Serial Number:      \"%s\"\n", board->serial_number.c_str());
						outbuf += stdsprintf("  Part Number:        \"%s\"\n", board->part_number.c_str());
						outbuf += stdsprintf("  FRU File ID:        \"%s\"\n", board->fru_file_id.c_str());
						if (board->custom_info.size()) {
							outbuf += "  Custom Info:\n";
							for (auto it = board->custom_info.begin(), eit = board->custom_info.end(); it != eit; ++it) {
								outbuf += std::string("    \"") + *it + "\"\n";
							}
						}
					}
					console->write(outbuf);
				}

				if (storage->getProductInfoAreaOffset()) {
					std::shared_ptr<RemoteFRUStorage::ProductInfoArea> product = storage->readProductInfoArea();
					std::string outbuf = "Product Info Area:\n";
					if (!product) {
						outbuf += "  Unreadable.\n";
					} else {
						outbuf += stdsprintf("  Product Info Area Version: %hhu\n", product->info_area_version);
						if (RemoteFRUStorage::language_codes.count(product->language_code)) {
							outbuf += stdsprintf("  Language Code:      0x%02hhx \"%s\"\n", product->language_code, RemoteFRUStorage::language_codes.at(product->language_code).c_str());
						} else {
							outbuf += stdsprintf("  Language Code:      0x%02hhx\n", product->language_code);
						}

						outbuf += stdsprintf("  Manufacturer:       \"%s\"\n", product->manufacturer.c_str());
						outbuf += stdsprintf("  Product Name:       \"%s\"\n", product->product_name.c_str());
						outbuf += stdsprintf("  Product Part/Model: \"%s\"\n", product->product_partmodel_number.c_str());
						outbuf += stdsprintf("  Product Version:    \"%s\"\n", product->product_version.c_str());
						outbuf += stdsprintf("  Serial Number:      \"%s\"\n", product->serial_number.c_str());
						outbuf += stdsprintf("  Asset Tag:          \"%s\"\n", product->asset_tag.c_str());
						outbuf += stdsprintf("  FRU File ID:        \"%s\"\n", product->fru_file_id.c_str());
						if (product->custom_info.size()) {
							outbuf += "  Custom Info:\n";
							for (auto it = product->custom_info.begin(), eit = product->custom_info.end(); it != eit; ++it) {
								outbuf += std::string("    \"") + *it + "\"\n";
							}
						}
					}
					console->write(outbuf);
				}
				if (storage->getMultirecordAreaOffset()) {
					if (!all) {
						console->write("Multi-Record Area: Present\n");
					} else {
						console->write("Multi-Record Area:\n");
						std::vector< std::vector<uint8_t> > records = storage->readMultiRecordArea();
						if (records.empty()) {
							console->write("  Read error.\n");
						} else {
							for (auto it = records.begin(), eit = records.end(); it != eit; ++it) {
								std::string recstr = " ";
								for (auto sit = it->begin(), esit = it->end(); sit != esit; ++sit) {
									recstr += stdsprintf(" %02x", *sit);
								}
								console->write(recstr + "\n");
							}
						}
					}
				}
			}
			else {
				// Invalid Header

				std::vector<uint8_t> frubuf = storage->readData(0, storage->getStorageAreaSize(), [console, storage](uint16_t offset, uint16_t size) {
					if ((offset % 0x200) == 0) {
						console->write(stdsprintf("Reading FRU Storage... %5hxh/%hxh\n", storage->getStorageAreaSize()-size, storage->getStorageAreaSize()));
					}
				});

				if (frubuf.size()) {
					std::string outbuf = "";
					for (std::vector<uint8_t>::size_type i = 0; i < frubuf.size(); ++i) {
						outbuf += stdsprintf("%02x", frubuf.at(i));
						if (i%16 == 15) {
							outbuf += "\n";
						} else if (i%4 == 3) {
							outbuf += "  ";
						} else {
							outbuf += " ";
						}
					}
					console->write(outbuf+"\n");
				} else {
					console->write("Attempts to read FRU storage failed.\n");
				}
			}
		});
	}

private:
	IPMBSvc &ipmb;
};

//! A "status" console command.
class IPMBSvc::StatusCommand : public CommandParser::Command {
public:
	StatusCommand(IPMBSvc &ipmb) : ipmb(ipmb) { };

	virtual std::string get_helptext(const std::string &command) const {
		return command + "\n\n"
				"Get the IPMBSvc status.\n";
	}

	virtual void execute(std::shared_ptr<ConsoleSvc> console, const CommandParser::CommandParameters &parameters) {
		console->write(stdsprintf("IPMB Address: 0x%02hhx\n", this->ipmb.kIPMBAddress));
	}

private:
	IPMBSvc &ipmb;
};

void IPMBSvc::registerConsoleCommands(CommandParser &parser, const std::string &prefix) {
	parser.registerCommand(prefix + "sendmsg", std::make_shared<IPMBSvc::SendMsgCommand>(*this));
	parser.registerCommand(prefix + "enumerate_fru_storages", std::make_shared<IPMBSvc::EnumerateFruStoragesCommand>(*this));
	parser.registerCommand(prefix + "dump_fru_storage", std::make_shared<IPMBSvc::DumpFruStorageCommand>(*this));
	parser.registerCommand(prefix + "status", std::make_shared<IPMBSvc::StatusCommand>(*this));
}

void IPMBSvc::deregisterConsoleCommands(CommandParser &parser, const std::string &prefix) {
	parser.registerCommand(prefix + "sendmsg", nullptr);
	parser.registerCommand(prefix + "enumerate_fru_storages", nullptr);
	parser.registerCommand(prefix + "dump_fru_storage", nullptr);
	parser.registerCommand(prefix + "status", nullptr);
}
