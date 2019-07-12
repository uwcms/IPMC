/*
 * AD7689.cpp
 *
 *  Created on: Sep 19, 2018
 *      Author: mpv
 */

#include <drivers/ad7689/AD7689.h>
#include <services/console/ConsoleSvc.h>

AD7689::AD7689(uint16_t DeviceId, const std::string &Name, uint32_t SlaveInterface) :
ADC(16, Name), SlaveInterface(SlaveInterface) {

	// Initialize the low level driver
	configASSERT(XST_SUCCESS == AD7689_S_Initialize(&(this->adc), DeviceId));

	// Apply default configurations
	this->setSamplingFrequency(1000);
}

AD7689::~AD7689() {
	// TODO
}

void AD7689::setSamplingFrequency(uint32_t hz) {
	AD7689_S_Set_Conv_Freq(&(this->adc), hz);
}

float AD7689::getTemperature() {
	// Channel 8 is the internal temperature monitor
	return this->readVolts(8) * 25000.0 / 283.0;
}

const uint32_t AD7689::readRaw(size_t channel) const {
	uint16_t val;
	// TODO: Throw if invalid channel
	AD7689_S_Get_Reading(&(this->adc), this->SlaveInterface, channel, &val);
	return val;
}

const float AD7689::readVolts(size_t channel) const {
	return this->rawToVolts(this->readRaw(channel));
}

const uint32_t AD7689::voltsToRaw(float volts) const {
	return ((uint16_t)((volts) / 2500.0 * 65545.0 * 1000.0));
}

const float AD7689::rawToVolts(uint32_t raw) const {
	return ((float)(raw) * 2500.0 / 65535.0 / 1000.0);
}

/// A "adc.override" console command.
class adc_override : public CommandParser::Command {
public:
	AD7689 &adc; ///< AD7689 object.

	///! Construct the command.
	adc_override(AD7689 &adc) : adc(adc) { };

	virtual std::string get_helptext(const std::string &command) const {
		return command + "$channel $off|value \n\n"
				"Override a specific ADC channel.\n";
	}

	virtual void execute(std::shared_ptr<ConsoleSvc> console, const CommandParser::CommandParameters &parameters) {
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
			ovrrd &= ~((1 << channel) << (adc.SlaveInterface * 8));
			AD7689_S_Set_Ch_Ovrrd_Enables(&(adc.adc), ovrrd);
		} else {
			unsigned int val;

			if (!CommandParser::CommandParameters::parse_one(value, &val)) {
				console->write("Invalid value.\n");
				return;
			}

			AD7689_S_Set_Ovrrd_Val(&(adc.adc), adc.SlaveInterface, channel, val);

			uint32_t ovrrd = AD7689_S_Get_Ch_Ovrrd_Enables(&(adc.adc));
			ovrrd |= ((1 << channel) << (adc.SlaveInterface * 8));
			AD7689_S_Set_Ch_Ovrrd_Enables(&(adc.adc), ovrrd);
		}
	}
};

/**
 * Register console commands related to this storage.
 * @param parser The command parser to register to.
 * @param prefix A prefix for the registered commands.
 */
void AD7689::register_console_commands(CommandParser &parser, const std::string &prefix) {
	parser.register_command(prefix + "override", std::make_shared<adc_override>(*this));
}

/**
 * Unregister console commands related to this storage.
 * @param parser The command parser to unregister from.
 * @param prefix A prefix for the registered commands.
 */
void AD7689::deregister_console_commands(CommandParser &parser, const std::string &prefix) {
	parser.register_command(prefix + "override", NULL);
}
