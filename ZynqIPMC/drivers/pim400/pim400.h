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

#ifndef SRC_COMMON_ZYNQIPMC_DRIVERS_PIM400_PIM400_H_
#define SRC_COMMON_ZYNQIPMC_DRIVERS_PIM400_PIM400_H_

#include <drivers/generics/i2c.h>
#include <services/console/ConsoleSvc.h>

/**
 * PIM400 power module I2C interface driver.
 *
 * Provides a low-level API function set to retrieve status from a PIM module.
 */
class PIM400 final : public ConsoleCommandSupport {
public:
	/**
	 * Initialize the PIM400 interface driver.
	 * @param i2c I2C interface to where the PIM400 is connected to.
	 * @param addr PIM400 I2C address (does NOT require to be shifted!).
	 * @thorw std::domain_error if address is invalid.
	 */
	PIM400(I2C &i2c, uint8_t addr);
	virtual ~PIM400();

	float getHoldupVoltage();	///< Read the hold-up voltage in 0.398V steps
	float getOutCurrent();		///< Read the output current in 0.094A steps
	float getFeedAVoltage();	///< Read the feed A voltage in 0.325V steps
	float getFeedBVoltage();	///< Read the feed B voltage in 0.325V steps
	float getTemperature();	///< Read the module temperature in 1.961C steps

	//! PIM400 status register bits, used in PIM400::read_status below.
	typedef struct {
		union {
			struct {
				bool enable_af :1;				///< Feed A is enabled
				bool enable_bf :1;				///< Feed B is enabled
				bool alarm_set :1;				///< Internal alarm is set
				bool :1;
				bool hdlp_connected :1;			///< Holdup capacitor is present
				bool hotswap_on :1;				///< Hotswap switch is on
				bool out_volt_undervoltage :1;	///< -48V_OUT is below threshold
				bool :1;
			};
			uint8_t _raw;
		};
	} PIM400Status;

	///< Read the status register
	PIM400Status getStatus();

	// From base class ConsoleCommandSupport:
	virtual void registerConsoleCommands(CommandParser &parser, const std::string &prefix="");
	virtual void deregisterConsoleCommands(CommandParser &parser, const std::string &prefix="");

private:
	class Status;

	I2C &i2c;				///< I2C bus that is wired to the PIM400.
	const uint8_t kAddr;	///< I2C address of the PIM400.

	//! Available PIM400 internal register.
	enum PIM400Register {
		PIM400_STATUS = 0x1E,
		PIM400_V_HLDP = 0x1F,
		PIM400_NEG48V_IOUT = 0x21,
		PIM400_NEG48V_AF = 0x22,
		PIM400_NEG48V_BF = 0x23,
		PIM400_TEMP = 0x28,
	};

	//! Do an internal register read operation.
	uint8_t readRegister(const PIM400Register reg);
};

#endif /* SRC_COMMON_ZYNQIPMC_DRIVERS_PIM400_PIM400_H_ */
