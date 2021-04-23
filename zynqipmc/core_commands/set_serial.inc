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

namespace CoreCommands {

/// A "set_serial" console command.
class SetSerialCommand : public CommandParser::Command {
public:
	SetSerialCommand()
	: eeprom(eeprom_mac), offset(0), serial_in_memory(IPMC_SERIAL), serial_description("IPMC") {};
	SetSerialCommand(EEPROM *eeprom, uint16_t offset, uint16_t &serial_in_memory, std::string serial_description)
	: eeprom(eeprom), offset(offset), serial_in_memory(serial_in_memory), serial_description(serial_description) {};

	EEPROM *eeprom;
	uint16_t offset;
	uint16_t &serial_in_memory;
	std::string serial_description;

	virtual std::string getHelpText(const std::string &command) const {
		return command + " $serial\n\n"
				"Set the " + this->serial_description + " serial number.\n"
				"NOTE: This cannot be changed once set!\n";
	}

	virtual void execute(std::shared_ptr<ConsoleSvc> console, const CommandParser::CommandParameters &parameters) {
		uint16_t serial;
		if (!parameters.parseParameters(1, true, &serial)) {
			console->write("Please provide a serial number.\n");
			return;
		}

		this->serial_in_memory = serial;
		this->eeprom->write(this->offset, reinterpret_cast<uint8_t*>(&serial), sizeof(serial));
		console->write("Serial updated.  Reboot to lock.\n");
	}
};

}
