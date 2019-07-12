/*
 * ELM.cpp
 *
 *  Created on: Feb 21, 2019
 *      Author: mpv
 */

#include <IPMC.h>
#include <libs/ThreadingPrimitives.h>
#include "ELM.h"

ELM::ELM(UART *uart, GPIO *targetsel)
: uart(uart), targetsel(targetsel) {
	// TODO: Should be able to partially take
	configASSERT(this->uart);

	this->mutex = xSemaphoreCreateMutex();
	configASSERT(this->mutex);

	memset(this->channelMapping, 0, sizeof(this->channelMapping));

	// Start the digest thread
	UWTaskCreate("elmlink", TASK_PRIORITY_BACKGROUND, [this]() {
		Packet p = {0};

		while (true) {
			// Digest incoming data from the link
			p.state = Packet::WAITING_HEADER;

			int r = this->digestInput(p, pdMS_TO_TICKS(1000));

			if (r < 0) {
				printf("Packet timed out");
			} else {
				if (p.chksum != calculateChecksum(p)) {
					printf("Packet checksum mismatch");
				} else {
					// Valid packet
//					if (p.meta.flowctrl) {
//						// Send Ack
//						this->sendAck(p.meta.channel);
//					}

					MutexGuard<false> lock(this->mutex);
					if (this->channelMapping[p.meta.channel]) {
						// Channel exists
//						if (p.meta.ack) {
//							// Ack packet
//							// TODO: Deassert wait on channel
//							xSemaphoreGive(this->channelMapping[p.meta.channel]->sync);
//						} else {
							this->channelMapping[p.meta.channel]->recv(p.content, p.size);
//						}
					} else {
						printf("Packet to unmapped channel (%d)", p.meta.channel);
					}
				}
			}
		}
	});
}

ELM::~ELM() {
	vSemaphoreDelete(this->mutex);
}

/// A "elm.bootsource" console command.
class elm_bootsource : public CommandParser::Command {
public:
	ELM &elm; ///< ELM object.

	///! Construct the command.
	elm_bootsource(ELM &elm) : elm(elm) { };

	virtual std::string get_helptext(const std::string &command) const {
		return command + " [release|sdcard|flash]\n\n"
				"Overrides the ELM boot source.\n";
	}

	virtual void execute(std::shared_ptr<ConsoleSvc> console, const CommandParser::CommandParameters &parameters) {
		if (parameters.nargs() == 1) {
			if ((elm.targetsel->getBusDirection() & 0x03) == 0x03) {
				console->write("ELM override is disabled.\n");
			} else {
				if ((elm.targetsel->getBusValue() & 0x03) == 0x02) {
					console->write("ELM override set to sdcard.\n");
				} else {
					console->write("ELM override set to flash.\n");
				}
			}
		} else {
			if (!parameters.parameters[1].compare("release")) {
				// Set pins as inputs
				elm.targetsel->setBusDirection(0x3);
			} else if (!parameters.parameters[1].compare("sdcard")) {
				elm.targetsel->setBusValue(0x2);
				elm.targetsel->setBusDirection(0x0);
			} else if (!parameters.parameters[1].compare("flash")) {
				elm.targetsel->setBusValue(0x0);
				elm.targetsel->setBusDirection(0x0);
			} else {
				console->write("Invalid source, see help.\n");
			}
		}
	}
};

/// A "elm.quiesce" console command.
class elm_quiesce : public CommandParser::Command {
public:
	ELM &elm; ///< ELM object.

	///! Construct the command.
	elm_quiesce(ELM &elm) : elm(elm) { };

	virtual std::string get_helptext(const std::string &command) const {
		return command + "\n\n"
				"Send a quiesce request to the ELM to .\n";
	}

	virtual void execute(std::shared_ptr<ConsoleSvc> console, const CommandParser::CommandParameters &parameters) {
		elm.sendPacket(0, (uint8_t*)"q", 1);
	}
};

/**
 * Register console commands related to this storage.
 * @param parser The command parser to register to.
 * @param prefix A prefix for the registered commands.
 */
void ELM::register_console_commands(CommandParser &parser, const std::string &prefix) {
	if (this->targetsel)
		parser.register_command(prefix + "bootsource", std::make_shared<elm_bootsource>(*this));
	parser.register_command(prefix + "quiesce", std::make_shared<elm_quiesce>(*this));
}

/**
 * Unregister console commands related to this storage.
 * @param parser The command parser to unregister from.
 * @param prefix A prefix for the registered commands.
 */
void ELM::deregister_console_commands(CommandParser &parser, const std::string &prefix) {
	if (this->targetsel)
		parser.register_command(prefix + "bootsource", NULL);
}

// TODO: Proper linkage between channels
// TODO: Proper timeouts

/** TODO for tomorrow:
 * - _ptr needs to be adjusted after diff size
 * - Add auto alloc, dealloc to Packet?
 * Need timeout if not all bytes are received
 */

uint16_t ELM::calculateChecksum(const Packet &p) {
	uint16_t chksum = p.meta.value + p.size;
	for (size_t i = 0; i < p.size; i++)
		chksum += p.content[i];
	chksum ^= 0xFFFF; // Bit flip
	return chksum;
}

bool ELM::sendPacket(const Packet &p) {
	uint8_t header[] = {LINKPROTO_SOP, p.meta.value};
	uint16_t chksum = calculateChecksum(p);

	// Send message down the link
	MutexGuard<false> lock(this->mutex);
	this->uart->write(header, sizeof(header));
//	if (p.meta.ack == 0) {
		this->uart->write((uint8_t*)&p.size, sizeof(p.size));
		this->uart->write(p.content, p.size);
//	}
	this->uart->write((uint8_t*)&chksum, sizeof(chksum));

	return true;
}

bool ELM::sendPacket(unsigned int channel, uint8_t *data, uint16_t size) {
	Packet p = {0};

	if (channel > 32) return false;

	p.meta.channel = channel;
	p.size = size;
	p.content = data;

	return this->sendPacket(p);
}

//void ELM::Link::sendAck(uint8_t channel) {
//	Packet p = {0};
//	p.meta.ack = 1;
//	p.meta.channel = channel;
//
//	this->sendPacket(p);
//}

int ELM::digestInput(Packet &p, TickType_t timeout) {
	size_t rcv = 0;

	while (true) {
		switch (p.state) {
			case Packet::WAITING_HEADER: {
				uint8_t header = 0;
				rcv = this->uart->read(&header, sizeof(header));
				if (rcv != sizeof(header))
					return -1;

				if (header == LINKPROTO_SOP) {
					p.state = Packet::WAITING_METADATA;
				}
			} break;

			case Packet::WAITING_METADATA: {
				rcv = this->uart->read(&(p.meta.value), sizeof(Metadata), timeout);
				if (rcv != sizeof(Metadata))
					return -1;

//				if (p.meta.ack) {
//					p.state = Packet::COMPLETE;
//					return 0;
//				} else {
					p.state = Packet::WAITING_SIZE;
//				}
			} break;

			case Packet::WAITING_SIZE: {
				rcv = this->uart->read((uint8_t*)&(p.size), sizeof(p.size), timeout);
				if (rcv != sizeof(p.size))
					return -1;

				if (p.content) {
					delete p.content;
					p.content = NULL;
				}

				if (p.size == 0) {
					p.state = Packet::WAITING_CHKSUM;
				} else {
					p.content = new uint8_t[p.size];
					p.state = Packet::WAITING_CONTENT;
				}
			} break;

			case Packet::WAITING_CONTENT: {
				rcv = this->uart->read(p.content, p.size, timeout);
				if (rcv != p.size)
					return -1;

				p.state = Packet::WAITING_CHKSUM;
			} break;

			case Packet::WAITING_CHKSUM: {
				rcv = this->uart->read((uint8_t*)&(p.chksum), sizeof(p.chksum), timeout);
				if (rcv != sizeof(p.chksum))
					return -1;

				p.state = Packet::COMPLETE;

				return 0;
			} break;

			default: break; // Do nothing
		}
	}

	return -1;
}


void ELM::linkChannel(ELM::Channel *c) {
	MutexGuard<false> lock(this->mutex);

	if (c->channel > 31) return; // Invalid
	this->channelMapping[c->channel] = c;
}

void ELM::unlinkChannel(ELM::Channel *c) {
	MutexGuard<false> lock(this->mutex);

	if (c->channel > 31) return; // Invalid
	if (this->channelMapping[c->channel] == c) {
		this->channelMapping[c->channel] = nullptr;
	}
}

bool ELM::Channel::send(uint8_t *content, size_t size) {
	Packet p = {0};
//	p.meta.ack = this->flowctrl;
	p.meta.channel = this->channel;
	p.size = size;
	p.content = content;
	p.chksum = calculateChecksum(p);

//	BaseType_t syncRet = pdFALSE;

//	for (size_t i = 0; i < 3; i++) {
		this->elm.sendPacket(p);

//		if (this->flowctrl) {
//			// Wait for ack
//			syncRet = xSemaphoreTake(this->sync, pdMS_TO_TICKS(1000));
//			if (syncRet == pdTRUE) break;
//			p.meta.retry = 1;
//			printf("Didn't receive ACK packet");
//		} else {
//			// No flow control, just return
//			break;
//		}
//	}

//	if (this->flowctrl && (syncRet == pdFALSE))
//		return false;

	return true;
}


