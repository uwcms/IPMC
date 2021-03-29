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
#include <map>

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
		 * @param channel_name The short name of the channel, to be used as a
		 *                     socket filename on the ELM.  Limit 32 characters.
		 */
		Channel(ELM &elm, std::string channel_name) :
				elm(elm) {
			if (channel_name.size() > 32)
				throw std::domain_error("Channel name too long.");
			this->elm.linkChannel(this, channel_name);
		};
		virtual ~Channel() {
			this->elm.unlinkChannel(this);
		};

		/**
		 * Sends a packet down the channel to the ELM.
		 *
		 * @param content Content to send out.
		 * @param size Number of bytes in content.
		 *
		 * @return true if success, false otherwise.
		 */
		virtual bool send(const uint8_t *content, size_t size) {
			return this->send(std::string((const char *)content, size));
		}
		/// \overload
		virtual bool send(const std::string &data) {
			return this->elm.sendPacket(this->channel_id, data);
		}

	protected:
		/**
		 * Callback that gets executed when a packet is received in this channel.
		 *
		 * @param content Received data packet.
		 * @param size Total number of bytes in content.
		 *
		 * \note You will receive a complete and valid packet, but are not
		 *       guaranteed to receive all packets sent, in case of link errors.
		 */
		virtual void recv(const uint8_t *content, size_t size) = 0;

		ELM &elm;
		uint8_t channel_id; ///< Automatically set at link time.

		friend ELM;
	};

	// From base class ConsoleCommandSupport:
	virtual void registerConsoleCommands(CommandParser &parser, const std::string &prefix="");
	virtual void deregisterConsoleCommands(CommandParser &parser, const std::string &prefix="");

private:
	// Available console commands:
	class BootSource;	///< Change the ELM bootsource command.

	//! Send a packet through the ELM link.
	bool sendPacket(uint8_t channel, const std::string &data);

	//! A thread to process incoming input packets.
	void recvThread();

	//! Link a channel to this interface.
	void linkChannel(ELM::Channel *c, std::string channel_name);

	//! Unlink a channel to this interface.
	void unlinkChannel(ELM::Channel *c);

private:
	SemaphoreHandle_t mutex;	///< Mutex for internal synchronization.
	UART &uart;					///< ELM UART link interface.
	GPIO *targetsel;			///< GPIO interface for boot source selection.
	std::map<uint8_t, ELM::Channel*> channels;	///< Configurable channel mappings
	std::map<uint8_t, std::string> channel_index; ///< A list of channel name/id mappings, for the ELM.

	class LinkIndexChannel;
	LinkIndexChannel *link_index_channel;
};

#endif /* SRC_COMMON_ZYNQIPMC_DRIVERS_ELM_ELM_H_ */
