/*
 * bootimage.h
 *
 *  Created on: Apr 4, 2019
 *      Author: mpv
 */

#ifndef SRC_COMMON_ZYNQIPMC_CORE_CONSOLE_COMMANDS_BOOTTARGET_H_
#define SRC_COMMON_ZYNQIPMC_CORE_CONSOLE_COMMANDS_BOOTTARGET_H_

#include <drivers/generics/eeprom.h>

namespace {

/// A "boottarget" command
class ConsoleCommand_boottarget : public CommandParser::Command {
public:

	EEPROM *eeprom;

	ConsoleCommand_boottarget(EEPROM *eeprom) : eeprom(eeprom) {};

	virtual std::string get_helptext(const std::string &command) const {
		return command + " [fallback|A|B|test]\n\n"
				"Retrieve or set the IPMC boot target.";
	}

	virtual void execute(std::shared_ptr<ConsoleSvc> console, const CommandParser::CommandParameters &parameters) {
		uint8_t image = 0, original = 0;

		if (eeprom->read(2, &original, sizeof(uint8_t)) != 1) {
			console->write("Failed to read boot target from MAC EEPROM.\n");
			return;
		}

		if ((original & 0x03) > 2) image = 0;
		else image = original;

		if (parameters.nargs() == 1) {
			std::string msg = "Target boot image is ";

			if (image & 0x04) image = 3;

			switch (image) {
			case 0: msg += "fallback"; break;
			case 1: msg += "A"; break;
			case 2: msg += "B"; break;
			case 3: msg += "test"; break;
			default: msg += "not set"; break;
			}

			console->write(msg + ".\n");
		} else {
			uint8_t target = 0;

			if (!parameters.parameters[1].compare("fallback")) {
				target = 0;
			} else if (!parameters.parameters[1].compare("A")) {
				target = 1;
			} else if (!parameters.parameters[1].compare("B")) {
				target = 2;
			} else if (!parameters.parameters[1].compare("test")) {
				target = 0x04 | (image & 0x3);
			} else {
				console->write("Unknown image name, see help.\n");
			}

			// Only write to EEPROM if required
			if (target != original) {
				if (eeprom->write(2, &target, sizeof(uint8_t)) != 1) {
					console->write("Failed to set boot target in MAC EEPROM.\n");
					return;
				}
			}
		}
	}
};

}


#endif /* SRC_COMMON_ZYNQIPMC_CORE_CONSOLE_COMMANDS_BOOTTARGET_H_ */
