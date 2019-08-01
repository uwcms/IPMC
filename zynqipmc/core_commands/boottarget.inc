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

#include <drivers/generics/eeprom.h>

namespace CoreCommands {

/// A "boottarget" command
class BootTargetCommand : public CommandParser::Command {
public:
	BootTargetCommand(EEPROM *eeprom) : eeprom(eeprom) {};

	virtual std::string getHelpText(const std::string &command) const {
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

private:
	EEPROM *eeprom;
};

}
