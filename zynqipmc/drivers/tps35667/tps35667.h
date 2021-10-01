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

#ifndef SRC_COMMON_ZYNQIPMC_DRIVERS_TPS35667_H_
#define SRC_COMMON_ZYNQIPMC_DRIVERS_TPS35667_H_

#include <FreeRTOS.h>
#include <drivers/generics/i2c.h>
#include <libs/threading.h>
#include <semphr.h>
#include <services/console/command_parser.h>

class TPS35667 : public ConsoleCommandSupport {
public:
	/**
	 * Instantiate an TP35667 interface.
	 *
	 * @param i2cbus  The I2C bus the TP35667 is on
	 * @param address The address on the I2C bus
	 */
	TPS35667(I2C &i2cbus, uint8_t address);
	virtual ~TPS35667();

	/// Read & interpret the Vin register.
	virtual float readVin(AbsoluteTimeout timeout = (TickType_t)pdMS_TO_TICKS(100));

	/// Read & interpret the Iin register.
	virtual float readIin(AbsoluteTimeout timeout = (TickType_t)pdMS_TO_TICKS(100));

	/// Read & interpret the Vout register.
	virtual float readVout(AbsoluteTimeout timeout = (TickType_t)pdMS_TO_TICKS(100));

	/// Read & interpret the Iout register.
	virtual float readIout(AbsoluteTimeout timeout = (TickType_t)pdMS_TO_TICKS(100));

	/// Read & interpret the Pin register.
	virtual float readPin(AbsoluteTimeout timeout = (TickType_t)pdMS_TO_TICKS(100));

	/// Read & interpret the Pout register.
	virtual float readPout(AbsoluteTimeout timeout = (TickType_t)pdMS_TO_TICKS(100));

	/// Read & interpret the Temperature register.
	virtual float readTemperature(AbsoluteTimeout timeout = (TickType_t)pdMS_TO_TICKS(100));

	/**
	 * Read a 16bit register.
	 *
	 * @throws std::runtime_error on communication failures
	 */
	virtual uint16_t reg16read(uint8_t address, AbsoluteTimeout timeout);

	virtual void registerConsoleCommands(CommandParser &parser, const std::string &prefix = "");
	virtual void deregisterConsoleCommands(CommandParser &parser, const std::string &prefix = "");

protected:
	SemaphoreHandle_t mutex; ///< A mutex protecting chip access.
	I2C &i2cbus;             ///< The I2C bus this EEPROM is attached to.
	uint8_t address;         ///< The I2C address for this EEPROM.
};

#endif /* Include Guard */
