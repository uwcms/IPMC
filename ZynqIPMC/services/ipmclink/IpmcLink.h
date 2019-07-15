/*
 * IpmcLink.h
 *
 *  Created on: Nov 30, 2018
 *      Author: mpv
 */

#ifndef SRC_COMMON_UW_IPMC_SERVICES_IPMCLINK_IPMCLINK_H_
#define SRC_COMMON_UW_IPMC_SERVICES_IPMCLINK_IPMCLINK_H_

#include <drivers/generics/uart.h>

// TODO: Proper linkage between channels
// TODO: Proper timeouts

/** TODO for tomorrow:
 * - _ptr needs to be adjusted after diff size
 * - Add auto alloc, dealloc to Packet?
 * Need timeout if not all bytes are received
 */

/**
 * TODO
 **/
class IPMCLink {
public:
	IPMCLink(UART &uart);
	~IPMCLink();

	class Channel;

protected:
	static constexpr uint8_t LINKPROTO_SOP = 0xAA; // Start of packet

	struct Metadata {
		union {
			struct {
				uint8_t ack      :1;
				uint8_t flowctrl :1; // Set if a return ack is required.
				uint8_t _rsvd    :2; // Reserved
				uint8_t channel  :4;
			};
			uint8_t value;
		};
	};

	struct Packet {
		Metadata meta;
		uint16_t size;
		uint8_t* content;
		uint16_t chksum;

		enum States {
			WAITING_HEADER,
			WAITING_METADATA,
			WAITING_SIZE,
			WAITING_CONTENT,
			WAITING_CHKSUM,
			COMPLETE
		} state;
	};

	static uint16_t calculateChecksum(const Packet &p);

	void sendPacket(const Packet &p);
	void sendAck(uint8_t channel);

	int digestInput(Packet &p, TickType_t timeout);

	void linkChannel(IPMCLink::Channel *c);

	void unlinkChannel(IPMCLink::Channel *c);

private:
	SemaphoreHandle_t mutex;
	UART &uart;
	IPMCLink::Channel* channelMapping[32];

public:
	friend IPMCLink::Channel;
};

class IPMCLink::Channel {
public:
	Channel(IPMCLink &link, uint8_t channel, bool flowctrl) : link(link), channel(channel), flowctrl(flowctrl) {
		if (flowctrl) {
			this->sync = xSemaphoreCreateBinary();
		}
		link.linkChannel(this);
	};
	~Channel() {
		link.unlinkChannel(this);
		vSemaphoreDelete(this->sync);
	};

	virtual void recv(uint8_t *content, size_t size) = 0;

	bool send(uint8_t *content, size_t size);

protected:
	IPMCLink link;
	uint8_t channel;
	bool flowctrl;
	SemaphoreHandle_t sync;

public:
	friend IPMCLink;
};



#endif /* SRC_COMMON_UW_IPMC_SERVICES_IPMCLINK_IPMCLINK_H_ */
