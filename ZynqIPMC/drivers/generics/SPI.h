/*
 * SPI.h
 *
 *  Created on: Jan 8, 2018
 *      Author: jtikalsky
 */

#ifndef SRC_COMMON_UW_IPMC_DRIVERS_GENERICS_SPI_H_
#define SRC_COMMON_UW_IPMC_DRIVERS_GENERICS_SPI_H_

#include <drivers/AtomicitySupport.h>
#include <FreeRTOS.h>
#include <functional>
#include <xil_types.h>

#ifdef INCLUDE_DRIVER_COMMAND_SUPPORT
#include <services/console/ConsoleSvc.h>
#include <libs/Utils.h>
#endif

/**
 * An abstract SPI master driver.
 * Chain operations supported by AddressableAtomicitySupport::atomic.
 */
class SPIMaster : public AddressableAtomicitySupport {
public:
	/**
	 * This function will perform a SPI transfer in a blocking manner.
	 * \param chip     The chip select to enable.
	 * \param sendbuf  The data to send.
	 * \param recvbuf  A buffer for received data. Can be NULL if no data needs to be kept.
	 * \param bytes    The number of bytes to transfer.
	 * \param timeout  Timeout in ticks, default is portMAX_DELAY (wait forever).
	 * \return false on error, else true.
	 * \note This function is thread-safe.
	 */
	virtual bool transfer(u8 chip, const u8 *sendbuf, u8 *recvbuf, size_t bytes, TickType_t timeout = portMAX_DELAY) = 0;

	/**
	 * Execute a SPI transfer without selecting or de-selecting a device. Useful for chaining.
	 * @param sendbuf The data to send.
	 * @param recvbuf A buffer for received data. Can be NULL if no data needs to be kept.
	 * @param bytes Number of bytes to transfer.
	 * @param timeout Timeout in ticks, default is portMAX_DELAY (wait forever).
	 * @return flase on error, else true.
	 * @warning this function is *NOT* thread-safe but can, and should, be used
	 * inside AddressableAtomicitySupport::atomic safely.
	 */
	virtual bool transfer_unsafe(const u8 *sendbuf, u8 *recvbuf, size_t bytes, TickType_t timeout = portMAX_DELAY) = 0;

	//! Returns true if Quad SPI is supported.
	virtual bool isQuadSupported() = 0;

#ifdef INCLUDE_DRIVER_COMMAND_SUPPORT
	class ConsoleCommand_spimaster_transfer : public CommandParser::Command {
	public:
		SPIMaster &spi;

		ConsoleCommand_spimaster_transfer(SPIMaster &spi) : spi(spi) {};

		virtual std::string get_helptext(const std::string &command) const {
			return command + " $byte_count [$byte1 $byte2 ..]\n\n"
					"Low level QSPI/SPI data transfer. Byte declaration is optional.\n";
		}

		virtual void execute(std::shared_ptr<ConsoleSvc> console, const CommandParser::CommandParameters &parameters) {
			size_t length;
			uint8_t cmd;

			if (!parameters.parse_parameters(1, false, &length, &cmd)) {
				console->write("Invalid arguments\n");
				return;
			}

			if (parameters.nargs() - 2 > length) {
				console->write("Too many arguments\n");
				return;
			}

			uint8_t writeBuf[length] = {0}, readBuf[length] = {0};
			writeBuf[0] = cmd;

			for (size_t i = 0; i < parameters.nargs() - 2; i++) {
				if (!parameters.parse_parameters(3+i, false, writeBuf + i + 1)) {
					break;
				}
			}

			console->write("Sending:\n" + formatedHexString(writeBuf, length) + "\n");

			if (!spi.transfer(0, writeBuf, readBuf, length, pdMS_TO_TICKS(2000))) {
				console->write("Transfer failed\n");
				return;
			}

			console->write("Received:\n" + formatedHexString(readBuf, length));
		}
	};

	void register_console_commands(CommandParser &parser, const std::string &prefix="") {
		parser.register_command(prefix + "transfer", std::make_shared<ConsoleCommand_spimaster_transfer>(*this));
	}

	void deregister_console_commands(CommandParser &parser, const std::string &prefix="") {
		parser.register_command(prefix + "transfer", NULL);
	}
#endif
};

#endif /* SRC_COMMON_UW_IPMC_DRIVERS_GENERICS_SPI_H_ */
