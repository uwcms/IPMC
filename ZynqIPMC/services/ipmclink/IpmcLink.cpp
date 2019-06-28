/*
 * IpmcLink.cpp
 *
 *  Created on: Nov 30, 2018
 *      Author: mpv
 */

#include <IPMC.h>
#include <libs/ThreadingPrimitives.h>
#include <libs/Utils.h>

#include "IpmcLink.h"

IPMCLink::IPMCLink(UART &uart) : uart(uart) {
	this->mutex = xSemaphoreCreateMutex();
	configASSERT(this->mutex);

	memset(this->channelMapping, 0, sizeof(this->channelMapping));

	// Start the digest thread
	UWTaskCreate("ipmclink", TASK_PRIORITY_BACKGROUND, [this]() {
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
					if (p.meta.flowctrl) {
						// Send Ack
						this->sendAck(p.meta.channel);
					}

					MutexGuard<false> lock(this->mutex);
					if (this->channelMapping[p.meta.channel]) {
						// Channel exists
						if (p.meta.ack) {
							// Ack packet
							// TODO: Deassert wait on channel
							xSemaphoreGive(this->channelMapping[p.meta.channel]->sync);
						} else {
							this->channelMapping[p.meta.channel]->recv(p.content, p.size);
						}
					} else {
						printf("Packet to unmapped channel (%d)", p.meta.channel);
					}
				}
			}
		}
	});

}

IPMCLink::~IPMCLink() {
	vSemaphoreDelete(this->mutex);
};

uint16_t IPMCLink::calculateChecksum(const Packet &p) {
	uint16_t chksum = p.meta.value + p.size;
	for (size_t i = 0; i < p.size; i++)
		chksum += p.content[i];
	chksum ^= 0xFFFF; // Bit flip
	return chksum;
}

void IPMCLink::sendPacket(const Packet &p) {
	uint8_t header[] = {LINKPROTO_SOP, p.meta.value};
	uint16_t chksum = calculateChecksum(p);

	// Send message down the link
	MutexGuard<false> lock(this->mutex);
	this->uart.write(header, sizeof(header));
	if (p.meta.ack == 0) {
		this->uart.write((uint8_t*)&p.size, sizeof(p.size));
		this->uart.write(p.content, p.size);
		this->uart.write((uint8_t*)&chksum, sizeof(chksum));
	}
}

void IPMCLink::sendAck(uint8_t channel) {
	Metadata meta = {0};
	meta.ack = 1;
	meta.channel = channel;

	uint8_t header[] = {LINKPROTO_SOP, meta.value};

	MutexGuard<false> lock(this->mutex);
	this->uart.write(header, sizeof(header));
}

int IPMCLink::digestInput(Packet &p, TickType_t timeout) {
	size_t rcv = 0;

	while (true) {
		switch (p.state) {
			case Packet::WAITING_HEADER: {
				uint8_t header = 0;
				rcv = this->uart.read(&header, sizeof(header));
				if (rcv != sizeof(header))
					return -1;

				if (header == LINKPROTO_SOP) {
					p.state = Packet::WAITING_METADATA;
				}
			} break;

			case Packet::WAITING_METADATA: {
				rcv = this->uart.read(&(p.meta.value), sizeof(Metadata), timeout);
				if (rcv != sizeof(Metadata))
					return -1;

				if (p.meta.ack) {
					p.state = Packet::COMPLETE;
				} else {
					p.state = Packet::WAITING_SIZE;
				}
			} break;

			case Packet::WAITING_SIZE: {
				rcv = this->uart.read((uint8_t*)&(p.size), sizeof(p.size), timeout);
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
				rcv = this->uart.read(p.content, p.size, timeout);
				if (rcv != p.size)
					return -1;

				p.state = Packet::WAITING_CHKSUM;
			} break;

			case Packet::WAITING_CHKSUM: {
				rcv = this->uart.read((uint8_t*)&(p.chksum), sizeof(p.chksum), timeout);
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


void IPMCLink::linkChannel(IPMCLink::Channel *c) {
	MutexGuard<false> lock(this->mutex);

	if (c->channel > 31) return; // Invalid
	this->channelMapping[c->channel] = c;
}

void IPMCLink::unlinkChannel(IPMCLink::Channel *c) {
	MutexGuard<false> lock(this->mutex);

	if (c->channel > 31) return; // Invalid
	if (this->channelMapping[c->channel] == c) {
		this->channelMapping[c->channel] = nullptr;
	}
}

bool IPMCLink::Channel::send(uint8_t *content, size_t size) {
	Packet p = {0};
	p.meta.ack = this->flowctrl;
	p.meta.channel = this->channel;
	p.size = size;
	p.content = content;
	p.chksum = calculateChecksum(p);

	BaseType_t syncRet = pdFALSE;

	for (size_t i = 0; i < 10; i++) {
		this->link.sendPacket(p);

		if (this->flowctrl) {
			// Wait for ack
			syncRet = xSemaphoreTake(this->sync, pdMS_TO_TICKS(2000));
			if (syncRet == pdTRUE) break;
			printf("Didn't receive ACK packet");
		} else {
			// No flow control, just return
			break;
		}
	}

	if (this->flowctrl && (syncRet == pdFALSE))
		return false;

	return true;
}

