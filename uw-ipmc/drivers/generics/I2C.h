/*
 * I2C.h
 *
 *  Created on: Jan 8, 2018
 *      Author: jtikalsky
 */

#ifndef SRC_COMMON_UW_IPMC_DRIVERS_GENERICS_I2C_H_
#define SRC_COMMON_UW_IPMC_DRIVERS_GENERICS_I2C_H_

#include <FreeRTOS.h>
#include <semphr.h>
#include <functional>
#include <libs/ThreadingPrimitives.h>

#ifdef INCLUDE_DRIVER_COMMAND_SUPPORT
#include <services/console/ConsoleSvc.h>
#include <libs/printf.h>
#include <libs/Utils.h>
#endif

/**
 * A generic I2C driver interface.
 */
class I2C {
public:
	I2C() {
		this->mutex = xSemaphoreCreateMutex();
		configASSERT(this->mutex);
	};

	virtual ~I2C() {
		vSemaphoreDelete(this->mutex);
	};

	/**
	 * Read buffer from the I2C interface
	 *
	 * \param addr The target I2C slave address.
	 * \param buf The buffer to read into.
	 * \param len The maximum number of bytes to read.
	 * \param timeout The timeout for this read, in standard FreeRTOS format.
	 * \return The total number of bytes read, 0 if an error occurred.
	 * \note Wrap around I2C::chain if multiple threads have access to the interface.
	 */
	virtual size_t read(uint8_t addr, uint8_t *buf, size_t len, TickType_t timeout = portMAX_DELAY, bool repeatedStart = false) = 0;

	/**
	 * Write buffer to the I2C interface
	 *
	 * \param addr The target I2C slave address.
	 * \param buf The buffer to write from.
	 * \param len The maximum number of bytes to write.
	 * \param timeout The timeout for this read, in standard FreeRTOS format.
	 * \return The total number of bytes written, 0 if an error occurred.
	 * \note Wrap around I2C::chain if multiple threads have access to the interface.
	 */
	virtual size_t write(uint8_t addr, const uint8_t *buf, size_t len, TickType_t timeout = portMAX_DELAY, bool repeatedStart = false) = 0;

	/**
	 * If I2C interface is used by several devices then several read/write
	 * operations can be chained by wrapping around a lambda function inside
	 * chain function which will grant exclusive access to the interface
	 *
	 * @param lambda_func The lambda function to run
	 **/
	inline virtual void chain(std::function<void(void)> lambda_func) {
		MutexGuard<false> lock(this->mutex, true);
		lambda_func();
	}

#ifdef INCLUDE_DRIVER_COMMAND_SUPPORT
	class ConsoleCommand_i2c_send : public CommandParser::Command {
	public:
		I2C &i2c;

		ConsoleCommand_i2c_send(I2C &i2c) : i2c(i2c) {};

		virtual std::string get_helptext(const std::string &command) const {
			return command + " $slave_addr [$byte1 $byte2 ..]\n\n"
					"Send bytes to connected I2C slave. Byte declaration is optional.\n";
		}

		virtual void execute(std::shared_ptr<ConsoleSvc> console, const CommandParser::CommandParameters &parameters) {
			const size_t length = parameters.nargs() - 2;
			uint8_t slaveaddr;

			if (!parameters.parse_parameters(1, false, &slaveaddr)) {
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
					if (!parameters.parse_parameters(i+2, false, &value)) {
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
				if (!this->i2c.write(slaveaddr, NULL, 0, pdMS_TO_TICKS(2000))){
					console->write("i2c.write failed\n");
					return;
				}
			}
		}
	};

	class ConsoleCommand_i2c_recv : public CommandParser::Command {
	public:
		I2C &i2c;

		ConsoleCommand_i2c_recv(I2C &i2c) : i2c(i2c) {};

		virtual std::string get_helptext(const std::string &command) const {
			return command + " $slave_addr $byte_count\n\n"
					"Receive bytes from connected I2C slave. $byte_count is the number of bytes to read.\n";
		}

		virtual void execute(std::shared_ptr<ConsoleSvc> console, const CommandParser::CommandParameters &parameters) {
			size_t length;
			uint8_t slaveaddr;

			if (!parameters.parse_parameters(1, true, &slaveaddr, &length)) {
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
				if (!this->i2c.read(slaveaddr, NULL, 0, pdMS_TO_TICKS(2000))){
					console->write("i2c.read failed\n");
					return;
				}

			}
		}
	};

	void register_console_commands(CommandParser &parser, const std::string &prefix="") {
		parser.register_command(prefix + "send", std::make_shared<ConsoleCommand_i2c_send>(*this));
		parser.register_command(prefix + "recv", std::make_shared<ConsoleCommand_i2c_recv>(*this));
	}

	void deregister_console_commands(CommandParser &parser, const std::string &prefix="") {
		parser.register_command(prefix + "send", NULL);
		parser.register_command(prefix + "recv", NULL);
	}
#endif

protected:
	SemaphoreHandle_t mutex;
};

#endif /* SRC_COMMON_UW_IPMC_DRIVERS_GENERICS_I2C_H_ */
