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

// TODO: Add console command support to this driver.

#ifndef SRC_COMMON_ZYNQIPMC_DRIVERS_PMBUS_PMBUS_H_
#define SRC_COMMON_ZYNQIPMC_DRIVERS_PMBUS_PMBUS_H_

#include <map>
#include <vector>
#include <drivers/generics/i2c.h>

/**
 * Standard PMBus driver that should be able to talk with most devices.
 *
 * The class can be derived to support devices with a more elaborate interface
 * or non-standard commands.
 */
class PMBus {
public:
	//! List of supported commands. Check the PMBus spec for more info.
	enum Command : uint8_t {
		VOUT_MODE = 0x20,

		READ_VIN = 0x88,
		READ_VOUT = 0x8B,
		READ_IOUT = 0x8C,
		READ_TEMPERATURE_1 = 0x8D,
		READ_TEMPERATURE_2 = 0x8E,
		READ_DUTY_CYCLE = 0x94,
		READ_FREQUENCY = 0x95,
	};

	//! Allowed formats when sending commands.
	enum Format { LINEAR, DIRECT, CUSTOM };

	//! Allowed units when sending or receiving data.
	enum Unit { NONE, VOLT, AMPERE, MILLISECONDS, CELSIUS };
	static const std::string unitToString(Unit unit);

	//! Command definition
	struct CommandDetails {
		size_t length;			///< Command length in bytes.
		const std::string name;	///< The command name.
		enum Format format;		///< Input/output format.
		enum Unit unit;			///< Input/output unit type.
	};

	//! Mapping of all supported PMBus commands by this driver with their respective details.
	static const std::map<Command, CommandDetails> CommandInfo;

	/**
	 * Initialize the PMBus interface driver.
	 * @param i2c I2C interface to where the PMBus device is connected to.
	 * @param addr PMBus device I2C address (does NOT require to be shifted!).
	 * @thorw std::domain_error if address is invalid.
	 */
	PMBus(I2C &i2c, uint8_t addr);
	virtual ~PMBus();

	/**
	 * Send and read the return of a single command to the PMBus device.
	 * @param cmd Refer to PMBus::Command for list of available commands.
	 * @param opt Array of optional arguments to the command, check the PMBus spec for more info.
	 * @return Read value after conversion.
	 * @throw std::runtime_error in case of an error.
	 */
	double sendCommand(Command cmd, std::vector<uint8_t> *opt = nullptr);

private:
	I2C &i2c;				///< I2C bus that is wired to the PMBus device.
	const uint8_t kAddr;	///< I2C address of the PMBus device.
};

#endif /* SRC_COMMON_ZYNQIPMC_DRIVERS_PMBUS_PMBUS_H_ */
