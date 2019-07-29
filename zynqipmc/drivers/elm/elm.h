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

#ifndef SRC_COMMON_ZYNQIPMC_DRIVERS_ELM_ELM_H_
#define SRC_COMMON_ZYNQIPMC_DRIVERS_ELM_ELM_H_

#include <drivers/generics/gpio.h>
#include <drivers/generics/uart.h>
#include <FreeRTOS.h>
#include <semphr.h>
#include <services/console/command_parser.h>
#include <string>

/**
 * ELM driver that implements the software layers for ELM link and other features.
 *
 * The ELM link is composed of several independent bi-directional packet based
 * channels working in though a single UART link. Channels can be defined in user
 * code depending on that application.
 *
 * The ELM will have drivers in Linux that work similarly.
 */
class ELM final : public ConsoleCommandSupport {
public:
	/**
	 * Initialize the ELM interface driver.
	 * @param uart UART interface wired to the ELM.
	 * @param targetsel GPIO interface that is used to control ELM's boot source. Optional.
	 */
	ELM(UART &uart, GPIO *targetsel = nullptr);
	~ELM();

	//! ELM link channel interface which can take any form of operation depending on data.
	class Channel {
	public:
		/**
		 * Initializes and links the channel to target ELM interface.
		 * @param elm ELM interface where channel exists.
		 * @param channel Number of the channel to be assigned.
		 */
		Channel(ELM &elm, uint8_t channel) : elm(elm), kChannel(channel) {
//			if (flowctrl) {
//				this->sync = xSemaphoreCreateBinary();
//			}
			elm.linkChannel(this);
		};
//		Channel(IPMCLink &link, uint8_t channel, bool flowctrl) : link(link), channel(channel), flowctrl(flowctrl) {
		virtual ~Channel() {
//			link.unlinkChannel(this);
//			vSemaphoreDelete(this->sync);
		};

		/**
		 * Callback that gets executed when data is received in this channel.
		 * @param content Received data.
		 * @param size Total number of bytes in content.
		 */
		virtual void recv(uint8_t *content, size_t size) = 0;

		/**
		 * Sends a message down the channel to the ELM.
		 * @param content Content to send out.
		 * @param size Number of bytes in content.
		 * @return true if success, false otherwise.
		 */
		bool send(const uint8_t *content, size_t size);

	protected:
		ELM &elm;
		const uint8_t kChannel;
//		bool flowctrl;
//		SemaphoreHandle_t sync;

	public:
		friend ELM;
	};

	// From base class ConsoleCommandSupport:
	virtual void registerConsoleCommands(CommandParser &parser, const std::string &prefix="");
	virtual void deregisterConsoleCommands(CommandParser &parser, const std::string &prefix="");

private:
	// Available console commands:
	class BootSource;	///< Change the ELM bootsource command.
	class Quiesce;		///< Send a quiesce request to the ELM.

	static constexpr uint8_t LINKPROTO_SOP = 0x01; ///< Start of packet

	//! Metadata associated on every packet.
	struct Metadata {
		union {
			struct {
//				uint8_t ack      :1;
//				uint8_t flowctrl :1; // Set if a return ack is required.
//				uint8_t retry    :1; // This is a retry
//				uint8_t _rsvd    :1;
				uint8_t _rsvd    :4; // Reserved
				uint8_t channel  :4; ///< Target channel number
			};
			uint8_t value;
		};
	};

	//! Packet structure sent over the link.
	struct Packet {
		Metadata meta;
		uint16_t size;
		uint8_t* content;
		uint16_t chksum;

		//! Packat transit states.
		enum States {
			WAITING_HEADER,
			WAITING_METADATA,
			WAITING_SIZE,
			WAITING_CONTENT,
			WAITING_CHKSUM,
			COMPLETE
		} state;
	};

	//! Calculate the checksum used for packet validation.
	static uint16_t calculateChecksum(const Packet &p);

	//! Send a packet through the ELM link.
	bool sendPacket(const Packet &p);
	bool sendPacket(unsigned int channel, uint8_t *data, uint16_t size);
//	void sendAck(uint8_t channel);

	//! Process incoming input packet.
	int digestInput(Packet &p, TickType_t timeout);

	//! Link a channel to this interface.
	void linkChannel(ELM::Channel *c);

	//! Unlink a channel to this interface.
	void unlinkChannel(ELM::Channel *c);

private:
	SemaphoreHandle_t mutex;	///< Mutex for internal synchronization.
	UART &uart;					///< ELM UART link interface.
	GPIO *targetsel;			///< GPIO interface for boot source selection.
	ELM::Channel* channels[32];	///< Configurable channel mapping
};

#endif /* SRC_COMMON_ZYNQIPMC_DRIVERS_ELM_ELM_H_ */
