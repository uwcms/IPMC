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

// Only include driver if the AD7689 is detected in the BSP.
#if XSDK_INDEXING || __has_include("ad7689_s.h")

#include "ad7689.h"
#include <services/console/ConsoleSvc.h>
#include <libs/except.h>

AD7689::AD7689(uint16_t device_id, const std::string &identifier, uint32_t slave_interface) :
ADC(16, identifier), kSlaveInterface(slave_interface) {
	// Initialize the low level driver
	if (AD7689_S_Initialize(&(this->adc), device_id) != XST_SUCCESS) {
		throw except::hardware_error("Unable to initialize AD7689(device_id=" + std::to_string(device_id) + ")");
	}

	if (slave_interface > this->adc.SlaveCount) {
		throw std::out_of_range("Slave interface number (" + std::to_string(slave_interface) + ") for AD7689(device_id=" + std::to_string(device_id) + ") is out-of-range");
	}

	// Apply default configurations
	this->setSamplingFrequency(1000);
}

AD7689::~AD7689() {}

void AD7689::setSamplingFrequency(uint32_t hz) {
	AD7689_S_Set_Conv_Freq(&(this->adc), hz);
}

float AD7689::getTemperature() {
	// Channel 8 is the internal temperature monitor
	return this->readVolts(8) * 25000.0 / 283.0;
}

const uint32_t AD7689::readRaw(size_t channel) const {
	uint16_t val;

	if (channel > 7) {
		throw std::out_of_range("Target channel ( " + std::to_string(channel) + ") is out-of-range, maximum is 7");
	}

	AD7689_S_Get_Reading(&(this->adc), this->kSlaveInterface, channel, &val);

	return val;
}

const uint32_t AD7689::voltsToRaw(float volts) const {
	return ((uint16_t)((volts) / 2500.0 * 65545.0 * 1000.0));
}

const float AD7689::rawToVolts(uint32_t raw) const {
	return ((float)(raw) * 2500.0 / 65535.0 / 1000.0);
}

void AD7689::registerConsoleCommands(CommandParser &parser, const std::string &prefix) {
	parser.register_command(prefix + "override", std::make_shared<Override>(*this));
}

void AD7689::deregisterConsoleCommands(CommandParser &parser, const std::string &prefix) {
	parser.register_command(prefix + "override", NULL);
}

std::string AD7689::Override::get_helptext(const std::string &command) const {
	return command + "$channel $off|hex_value\n\n"
			"Override a specific ADC channel.\n\n"
			"Examples:\n"
			" Set channel 2 to maximum value: " + command + " 2 0xffff\n"
			" Turn off channel overriding:    " + command + " 2 off\n";
}

void AD7689::Override::execute(std::shared_ptr<ConsoleSvc> console, const CommandParser::CommandParameters &parameters) {
	unsigned int channel;
	std::string value;

	if (!parameters.parse_parameters(1, false, &channel, &value)) {
		console->write("Invalid parameters, see help.\n");
		return;
	}

	if (channel > 7) {
		console->write("Channel out-of-range.\n");
		return;
	}

	if (value.compare("off") == 0) {
		// Disabled
		uint32_t ovrrd = AD7689_S_Get_Ch_Ovrrd_Enables(&(adc.adc));
		ovrrd &= ~((1 << channel) << (adc.kSlaveInterface * 8));
		AD7689_S_Set_Ch_Ovrrd_Enables(&(adc.adc), ovrrd);
	} else {
		unsigned int val;

		if (!CommandParser::CommandParameters::parse_one(value, &val)) {
			console->write("Invalid value.\n");
			return;
		}

		AD7689_S_Set_Ovrrd_Val(&(adc.adc), adc.kSlaveInterface, channel, val);

		uint32_t ovrrd = AD7689_S_Get_Ch_Ovrrd_Enables(&(adc.adc));
		ovrrd |= ((1 << channel) << (adc.kSlaveInterface * 8));
		AD7689_S_Set_Ch_Ovrrd_Enables(&(adc.adc), ovrrd);
	}
}

#endif
