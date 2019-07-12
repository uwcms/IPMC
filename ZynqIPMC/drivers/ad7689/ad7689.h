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

#ifndef SRC_COMMON_ZYNQIPMC_DRIVERS_AD7689_AD7689_H_
#define SRC_COMMON_ZYNQIPMC_DRIVERS_AD7689_AD7689_H_

// Only include driver if the AD7689 is detected in the BSP.
#if XSDK_INDEXING || __has_include(<ad7689_s.h>)

#include "ad7689_s.h"
#include <drivers/generics/ADC.h>
#include <services/console/ConsoleSvc.h>
#include <services/console/CommandParser.h>

/**
 * Driver for the AD7689 PL firmware IP.
 * Each object references a single AD7689 chip connected to the PL IP.
 * If in firmware the IP has multiple slaves then one object per slave interface
 * is required, refer to the constructor on how to do this.
 */
class AD7689 final : public ADC, public ConsoleCommandSupport {
public:
	/**
	 * Interfaces with a single AD7689 chip through an AD7689 PL IP and slave interface.
	 * @param device_id AD7689 device ID, normally starts at 0.
	 * @param name Custom name to identify the ADC object.
	 * @param slave_interface Target slave interface, defined in the firmware.
	 * @throw std::out_of_range if slave_interface is outside range.
	 * @throw except::hardware_error if low level driver fails to configure.
	 */
	AD7689(uint16_t device_id, const std::string &identifier, uint32_t slave_interface = 0);
	virtual ~AD7689();

	//! Set the ADC sampling frequency in Hertz.
	void setSamplingFrequency(uint32_t hz);

	//! Returns the ADC internal temperature in Celsius.
	float getTemperature();

	// From parent class ADC:
	virtual const uint32_t readRaw(size_t channel) const;
	virtual const float readVolts(size_t channel) const;
	virtual const uint32_t voltsToRaw(float volts) const;
	virtual const float rawToVolts(uint32_t raw) const;

	// From parent class ConsoleCommandSupport:
	virtual void registerConsoleCommands(CommandParser &parser, const std::string &prefix="");
	virtual void deregisterConsoleCommands(CommandParser &parser, const std::string &prefix="");

	//! Allows to override the ADC values from the console, useful for testing and debugging.
	class Override : public CommandParser::Command {
	public:
		Override(AD7689 &adc) : adc(adc) { };
		virtual std::string get_helptext(const std::string &command) const;
		virtual void execute(std::shared_ptr<ConsoleSvc> console, const CommandParser::CommandParameters &parameters);
	private:
		AD7689 &adc;
	};

private:
	mutable AD7689_S adc = {0};	///< Internal ADC driver data.
	uint32_t kSlaveInterface;	///< Target slave interface.
};

#endif

#endif /* SRC_COMMON_ZYNQIPMC_DRIVERS_AD7689_AD7689_H_ */
