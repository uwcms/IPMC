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

#ifndef SRC_COMMON_UW_IPMC_DRIVERS_LTC2654_LTC2654_H_
#define SRC_COMMON_UW_IPMC_DRIVERS_LTC2654_LTC2654_H_

#include <stdint.h>
#include <drivers/generics/spi.h>
#include <services/console/CommandParser.h>

/**
 * High-level driver for the LTC2654 Quad 16/12bit rail-to-rail DAC.
 * Each channel (A/B/C/D) is individually configured.
 * @note The value of DAC ratio is written but only takes affect after an update.
 */
class LTC2654 final : public ConsoleCommandSupport {
public:
	//! Addressing mode.
	enum Address : uint8_t {
		DAC_A = 0x00,
		DAC_B = 0x01,
		DAC_C = 0x02,
		DAC_D = 0x03,
		ALL_DACS = 0x0f
	};

	//! Available commands.
	enum Command : uint8_t {
		WRITE_INPUT_REG = 0x00,					///< Write to target DAC but do not update yet.
		UPDATE_DAC_REG = 0x01,					///< Update target DAC.
		WRITE_INPUT_REG_AND_UPDATE_ALL = 0x02,	///< Write to target DAC and update all others too.
		WRITE_AND_UPDATE_REG = 0x03,			///< Write and update target DAC only.
		POWER_DOWN = 0x04,						///< Power down target DAC.
		POWER_DOWN_CHIP = 0x05,					///< Power down the whole chip.
		SELECT_INTERNAL_REF = 0x06,				///< Select internal reference.
		SELECT_EXTERNAL_REF = 0x07,				///< Select external reference.
		NO_OP = 0x0f							///< No operation.
	};

	/**
	 * Initializes the LTC2654 driver.
	 * @param spi SPI interface to where the LTC2654 is wired to.
	 * @param chip_select Its corresponding chip select in the SPI interface.
	 * @param is12Bits Set to true if this variant of the LTC2654 supports 12bits.
	 */
	LTC2654(SPIMaster& spi, size_t chip_select, bool is12Bits);
	~LTC2654() {};

	/**
	 * Send a command to the LTC2654. See the list of available commands.
	 * @param addr Target DAC, can be set to ALL_DACS.
	 * @param cmd Desired command, see enum Command.
	 * @param val Command argument
	 * @return true if success, false otherwise.
	 */
	bool sendCommand(Address addr, Command cmd, uint16_t val = 0);

	/**
	 * Sets the DAC value based on a ratio.
	 * @param addr Target DAC, can be set to ALL_DACS.
	 * @param ratio Ratio from 0.0 to 1.0
	 * @throw std::out_of_range if ratio is out-of-range.
	 */
	void setDAC(Address addr, float ratio);

	// From base class ConsoleCommandSupport:
	virtual void registerConsoleCommands(CommandParser &parser, const std::string &prefix="");
	virtual void deregisterConsoleCommands(CommandParser &parser, const std::string &prefix="");

private:
	class Send;	///< Send a command to LTC2654;

	SPIMaster &spi;				///< SPI interface to where the DAC is wired to.
	const size_t kChipSelect;	///< Its chip select in the SPI interface.
	const bool kIs12Bits;		///< true if the DAC has 12bits instead of 16.
};

#endif /* SRC_COMMON_UW_IPMC_DRIVERS_LTC2654_LTC2654_H_ */
