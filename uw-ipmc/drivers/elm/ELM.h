/*
 * ELM.h
 *
 *  Created on: Feb 21, 2019
 *      Author: mpv
 */

#ifndef SRC_COMMON_UW_IPMC_DRIVERS_ELM_ELM_H_
#define SRC_COMMON_UW_IPMC_DRIVERS_ELM_ELM_H_

#include <FreeRTOS.h>
#include <semphr.h>
#include <string>
#include <drivers/generics/UART.h>
#include <drivers/pl_gpio/PLGPIO.h>
#include <services/console/CommandParser.h>
#include <services/console/ConsoleSvc.h>

/**
 * ELM driver that implements the software layers for ELM link and other features.
 */
class ELM {
public:
	ELM(UART *uart, PL_GPIO *gpio);
	virtual ~ELM();

	class Channel;

protected:
	friend class elm_bootsource;
	friend class elm_quiesce;

public:
	void register_console_commands(CommandParser &parser, const std::string &prefix="");
	void deregister_console_commands(CommandParser &parser, const std::string &prefix="");

protected:
	static constexpr uint8_t LINKPROTO_SOP = 0x01; // Start of packet

	struct Metadata {
		union {
			struct {
//				uint8_t ack      :1;
//				uint8_t flowctrl :1; // Set if a return ack is required.
//				uint8_t retry    :1; // This is a retry
//				uint8_t _rsvd    :1;
				uint8_t _rsvd    :4; // Reserved
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

	bool sendPacket(const Packet &p);
	bool sendPacket(unsigned int channel, uint8_t *data, uint16_t size);
//	void sendAck(uint8_t channel);

	int digestInput(Packet &p, TickType_t timeout);

	void linkChannel(ELM::Channel *c);

	void unlinkChannel(ELM::Channel *c);

private:
	SemaphoreHandle_t mutex;
	UART *uart;
	PL_GPIO *gpio;
	ELM::Channel* channelMapping[32];
};

class ELM::Channel {
public:
//	Channel(IPMCLink &link, uint8_t channel, bool flowctrl) : link(link), channel(channel), flowctrl(flowctrl) {
	Channel(ELM &elm, uint8_t channel) : elm(elm), channel(channel) {
//		if (flowctrl) {
//			this->sync = xSemaphoreCreateBinary();
//		}
		elm.linkChannel(this);
	};
	virtual ~Channel() {
//		link.unlinkChannel(this);
//		vSemaphoreDelete(this->sync);
	};

	virtual void recv(uint8_t *content, size_t size) = 0;

	bool send(uint8_t *content, size_t size);

	virtual int async_read(uint8_t *content, size_t size, TickType_t timeout, TickType_t data_timeout) = 0;

protected:
	ELM &elm;
	uint8_t channel;
//	bool flowctrl;
//	SemaphoreHandle_t sync;

public:
	friend ELM;
};


#endif /* SRC_COMMON_UW_IPMC_DRIVERS_ELM_ELM_H_ */
