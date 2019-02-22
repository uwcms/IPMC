/*
 * ELM.cpp
 *
 *  Created on: Feb 21, 2019
 *      Author: mpv
 */

#include "ELM.h"

ELM::ELM(UART *uart, PL_GPIO *gpio)
: uart(uart), gpio(gpio) {
	configASSERT(this->uart);
	configASSERT(this->gpio);
}

ELM::~ELM() {
}

/// A "elm.bootsource" console command.
class elm_bootsource : public CommandParser::Command {
public:
	ELM &elm; ///< ELM object.

	///! Construct the command.
	elm_bootsource(ELM &elm) : elm(elm) { };

	virtual std::string get_helptext(const std::string &command) const {
		return command + " [release|sdcard|flash]\n\n"
				"Overrides the ELM boot source.\n";
	}

	virtual void execute(std::shared_ptr<ConsoleSvc> console, const CommandParser::CommandParameters &parameters) {
		if (parameters.nargs() == 1) {
			if ((elm.gpio->getDirection(PL_GPIO::GPIO_CHANNEL2) & 0x03) == 0x03) {
				console->write("ELM override is disabled.\n");
			} else {
				if ((elm.gpio->getChannel(PL_GPIO::GPIO_CHANNEL2) & 0x03) == 0x02) {
					console->write("ELM override set to sdcard.\n");
				} else {
					console->write("ELM override set to flash.\n");
				}
			}
		} else {
			if (!parameters.parameters[1].compare("release")) {
				// Set pins as inputs
				elm.gpio->setDirection(0x3, PL_GPIO::GPIO_CHANNEL2);
			} else if (!parameters.parameters[1].compare("sdcard")) {
				elm.gpio->setChannel(0x2, PL_GPIO::GPIO_CHANNEL2);
				elm.gpio->setDirection(0x0, PL_GPIO::GPIO_CHANNEL2);
			} else if (!parameters.parameters[1].compare("flash")) {
				elm.gpio->setChannel(0x0, PL_GPIO::GPIO_CHANNEL2);
				elm.gpio->setDirection(0x0, PL_GPIO::GPIO_CHANNEL2);
				return;
			}
		}
	}
};

/**
 * Register console commands related to this storage.
 * @param parser The command parser to register to.
 * @param prefix A prefix for the registered commands.
 */
void ELM::register_console_commands(CommandParser &parser, const std::string &prefix) {
	parser.register_command(prefix + "bootsource", std::make_shared<elm_bootsource>(*this));
}

/**
 * Unregister console commands related to this storage.
 * @param parser The command parser to unregister from.
 * @param prefix A prefix for the registered commands.
 */
void ELM::deregister_console_commands(CommandParser &parser, const std::string &prefix) {
	parser.register_command(prefix + "bootsource", NULL);
}


