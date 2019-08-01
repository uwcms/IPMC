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

#ifndef SRC_COMMON_ZYNQIPMC_DRIVERS_GENERICS_SPI_H_
#define SRC_COMMON_ZYNQIPMC_DRIVERS_GENERICS_SPI_H_

#include <drivers/atomicity_support.h>
#include <libs/utils.h>
#include <services/console/command_parser.h>
#include <services/console/consolesvc.h>
#include <zynqipmc_config.h>

/**
 * An abstract SPI master driver.
 * Chain operations supported by AddressableAtomicitySupport::atomic.
 */
class SPIMaster : public AddressableAtomicitySupport, public ConsoleCommandSupport {
public:
	/**
	 * This function will perform a SPI transfer in a blocking manner.
	 * @param chip     The chip select to enable.
	 * @param sendbuf  Buffer with data to send on the bus.
	 * @param recvbuf  Buffer for received data. Can be nullptr if no data needs to be kept.
	 * @param bytes    Number of bytes to transfer.
	 * @param timeout  Timeout in ticks, default is portMAX_DELAY (wait forever).
	 * @return false on error, true on success.
	 * @note This function is thread-safe.
	 */
	virtual bool transfer(size_t chip, const uint8_t *sendbuf, uint8_t *recvbuf, size_t bytes, TickType_t timeout = portMAX_DELAY) = 0;

	/**
	 * Execute a SPI transfer without selecting or de-selecting a device. Useful for chaining.
	 * @param sendbuf Buffer with data to send on the bus.
	 * @param recvbuf Buffer for received data. Can be nullptr if no data needs to be kept.
	 * @param bytes Number of bytes to transfer.
	 * @param timeout Timeout in ticks, default is portMAX_DELAY (wait forever).
	 * @return false on error, true on success.
	 * @note This function is NOT thread-safe and will throw an error if driver hasn't locked the mutex beforehand.
	 * It is intended to be used inside ::atomic in chain operation which will do this automatically.
	 * The driver should check if using ::inAtomic before executing the rest of the code.
	 */
	virtual bool transfer(const uint8_t *sendbuf, uint8_t *recvbuf, size_t bytes, TickType_t timeout = portMAX_DELAY) = 0;

	//! Needs to return true if Quad SPI is supported in this interface implementation.
	virtual bool isQuadSupported() const = 0;

#ifdef ENABLE_DRIVER_COMMAND_SUPPORT
	class Transfer : public CommandParser::Command {
	public:
		Transfer(SPIMaster &spi) : spi(spi) {};

		virtual std::string getHelpText(const std::string &command) const {
			return command + " $byte_count [$byte1 $byte2 ..]\n\n"
					"Low level QSPI/SPI data transfer. Byte declaration is optional.\n";
		}

		virtual void execute(std::shared_ptr<ConsoleSvc> console, const CommandParser::CommandParameters &parameters) {
			size_t length;
			uint8_t cmd;

			if (!parameters.parseParameters(1, false, &length, &cmd)) {
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
				if (!parameters.parseParameters(3+i, false, writeBuf + i + 1)) {
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

	private:
		SPIMaster &spi;
	};
#endif

	virtual void registerConsoleCommands(CommandParser &parser, const std::string &prefix="") {
#ifdef ENABLE_DRIVER_COMMAND_SUPPORT
		parser.registerCommand(prefix + "transfer", std::make_shared<SPIMaster::Transfer>(*this));
#endif
	}

	virtual void deregisterConsoleCommands(CommandParser &parser, const std::string &prefix="") {
#ifdef ENABLE_DRIVER_COMMAND_SUPPORT
		parser.registerCommand(prefix + "transfer", nullptr);
#endif
	}
};

#endif /* SRC_COMMON_ZYNQIPMC_DRIVERS_GENERICS_SPI_H_ */
