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

#ifndef SRC_COMMON_ZYNQIPMC_DRIVERS_GENERICS_I2C_H_
#define SRC_COMMON_ZYNQIPMC_DRIVERS_GENERICS_I2C_H_

#include <drivers/atomicity_support.h>
#include <libs/utils.h>
#include <services/console/command_parser.h>
#include <services/console/consolesvc.h>

/**
 * An abstract I2C driver interface.
 */
class I2C : public AtomicitySupport, public ConsoleCommandSupport {
public:
	/**
	 * Read buffer from the I2C interface
	 *
	 * @param addr The target I2C slave address.
	 * @param buf The buffer to read into.
	 * @param len The maximum number of bytes to read.
	 * @param timeout The timeout for this read, in standard FreeRTOS format.
	 * @return The total number of bytes read, 0 if an error occurred.
	 * @note Wrap around I2C::chain if multiple threads have access to the interface.
	 */
	virtual size_t read(uint8_t addr, uint8_t *buf, size_t len, TickType_t timeout = portMAX_DELAY, bool repeatedStart = false) = 0;

	/**
	 * Write buffer to the I2C interface
	 *
	 * @param addr The target I2C slave address.
	 * @param buf The buffer to write from.
	 * @param len The maximum number of bytes to write.
	 * @param timeout The timeout for this read, in standard FreeRTOS format.
	 * @return The total number of bytes written, 0 if an error occurred.
	 * @note Wrap around I2C::chain if multiple threads have access to the interface.
	 */
	virtual size_t write(uint8_t addr, const uint8_t *buf, size_t len, TickType_t timeout = portMAX_DELAY, bool repeatedStart = false) = 0;

#ifdef ENABLE_DRIVER_COMMAND_SUPPORT
	class Send : public CommandParser::Command {
	public:
		Send(I2C &i2c) : i2c(i2c) {};

		virtual std::string getHelpText(const std::string &command) const {
			return command + " $slave_addr [$byte1 $byte2 ..]\n\n"
					"Send bytes to connected I2C slave. Byte declaration is optional.\n";
		}

		virtual void execute(std::shared_ptr<ConsoleSvc> console, const CommandParser::CommandParameters &parameters) {
			const size_t length = parameters.nargs() - 2;
			uint8_t slaveaddr;

			if (!parameters.parseParameters(1, false, &slaveaddr)) {
				console->write("Invalid arguments, see help.\n");
				return;
			}

			if (slaveaddr > 0x7f) {
				console->write("Slave address out of range, see help.\n");
				return;
			}

			if (length > 0) {
				std::unique_ptr<uint8_t> array(new uint8_t[length]);
				if (&*array == nullptr) {
					throw std::runtime_error("Out-of-memory");
				}

				for (size_t i = 0; i < length; i++) {
					uint8_t value;
					if (!parameters.parseParameters(i+2, false, &value)) {
						console->write("Cannot parse argument " + std::to_string(i+1) + ", see help.\n");
						return;
					}

					(&*array)[i] = value;
				}

				console->write("Sending:\n" + formatedHexString(&*array, length));

				if (!this->i2c.write(slaveaddr, &*array, length, pdMS_TO_TICKS(2000))){
					console->write("i2c.write failed\n");
					return;
				}
			} else {
				if (!this->i2c.write(slaveaddr, nullptr, 0, pdMS_TO_TICKS(2000))){
					console->write("i2c.write failed\n");
					return;
				}
			}
		}

	private:
		I2C &i2c;
	};

	class Recv : public CommandParser::Command {
	public:
		Recv(I2C &i2c) : i2c(i2c) {};

		virtual std::string getHelpText(const std::string &command) const {
			return command + " $slave_addr $byte_count\n\n"
					"Receive bytes from connected I2C slave. $byte_count is the number of bytes to read.\n";
		}

		virtual void execute(std::shared_ptr<ConsoleSvc> console, const CommandParser::CommandParameters &parameters) {
			size_t length;
			uint8_t slaveaddr;

			if (!parameters.parseParameters(1, true, &slaveaddr, &length)) {
				console->write("Invalid arguments, see help.\n");
				return;
			}

			if (slaveaddr > 0x7f) {
				console->write("Slave address out of range, see help.\n");
				return;
			}

			if (length > 0) {
				std::unique_ptr<uint8_t> array(new uint8_t[length]);
				if (&*array == nullptr) {
					throw std::runtime_error("Out-of-memory");
				}

				if (!this->i2c.read(slaveaddr, &*array, length, pdMS_TO_TICKS(2000))){
					console->write("i2c.read failed\n");
					return;
				}

				console->write("Received:\n" + formatedHexString(&*array, length));
			} else {
				if (!this->i2c.read(slaveaddr, nullptr, 0, pdMS_TO_TICKS(2000))){
					console->write("i2c.read failed\n");
					return;
				}

			}
		}

	private:
		I2C &i2c;
	};
#endif

	virtual void registerConsoleCommands(CommandParser &parser, const std::string &prefix="") {
#ifdef ENABLE_DRIVER_COMMAND_SUPPORT
		parser.registerCommand(prefix + "send", std::make_shared<I2C::Send>(*this));
		parser.registerCommand(prefix + "recv", std::make_shared<I2C::Recv>(*this));
#endif
	}

	virtual void deregisterConsoleCommands(CommandParser &parser, const std::string &prefix="") {
#ifdef ENABLE_DRIVER_COMMAND_SUPPORT
		parser.registerCommand(prefix + "send", nullptr);
		parser.registerCommand(prefix + "recv", nullptr);
#endif
	}

protected:
	SemaphoreHandle_t mutex;
};

#endif /* SRC_COMMON_ZYNQIPMC_DRIVERS_GENERICS_I2C_H_ */
