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

#include "ltc2654.h"
#include <libs/except.h>

LTC2654::LTC2654(SPIMaster& spi, size_t chip_select, bool is12Bits) :
spi(spi), kChipSelect(chip_select), kIs12Bits(is12Bits) {
}

bool LTC2654::sendCommand(Address addr, Command cmd, uint16_t val) {
	uint8_t data[3] = {0};

	data[0] = ((cmd << 4) & 0xF0) | (addr & 0x0F);
	if (this->kIs12Bits) {
		data[1] = val >> 4;
		data[2] = (val << 4) & 0xF0;
	} else {
		data[1] = (val >> 8);
		data[2] = val;
	}

	return this->spi.transfer(this->kChipSelect, data, NULL, sizeof(data), pdMS_TO_TICKS(1000));
}

void LTC2654::setDAC(Address addr, float ratio) {
	if ((ratio > 1.0) || (ratio < 0.0)) throw std::out_of_range("Range needs to be between 0.0 and 1.0");

	uint16_t val = (this->kIs12Bits?0x0fff:0xffff) * ratio;
	this->sendCommand(addr, WRITE_INPUT_REG, val);
}

//! Allows to set LTC2654 through the console interface.
class LTC2654::Send : public CommandParser::Command {
public:
	Send(LTC2654 &dac) : dac(dac) {};

	virtual std::string get_helptext(const std::string &command) const {
		return command + "%s <channel: A|B|C|D> <command: W|U|S|D> [val]\n\n"
				"Operate the DAC, check LTC2654 datasheet. Mid-point is 0x7ff at 12bits.\n"
				"Example usage: " + command + " B W 0x7ff\n";
	}

	virtual void execute(std::shared_ptr<ConsoleSvc> console, const CommandParser::CommandParameters &parameters) {
		if (parameters.nargs() < 3 || parameters.nargs() > 4) {
			console->write("Invalid arguments, see help.\n");
			return;
		}

		std::string arg = "";
		if (!parameters.parse_parameters(1, true, &arg) && arg.length() != 1) {
			console->write("Invalid DAC channel, see help.\n");
			return;
		}

		LTC2654::Address addr = LTC2654::ALL_DACS;
		switch (arg[0]) {
		case 'A': addr = LTC2654::DAC_A; break;
		case 'B': addr = LTC2654::DAC_B; break;
		case 'C': addr = LTC2654::DAC_C; break;
		case 'D': addr = LTC2654::DAC_D; break;
		default:
			console->write("Invalid DAC channel, see help.\n");
			return;
		}

		if (!parameters.parse_parameters(2, true, &arg) && arg.length() != 1) {
			console->write("Invalid command, see help.\n");
			return;
		}

		LTC2654::Command cmd = LTC2654::NO_OP;
		switch (arg[0]) {
		case 'W': cmd = LTC2654::WRITE_INPUT_REG; break;
		case 'U': cmd = LTC2654::UPDATE_DAC_REG; break;
		case 'S': cmd = LTC2654::WRITE_AND_UPDATE_REG; break;
		case 'D': cmd = LTC2654::POWER_DOWN; break;
		default:
			console->write("Invalid command, see help.\n");
			return;
		}

		uint32_t val = 0;
		if (cmd == LTC2654::WRITE_INPUT_REG || cmd == LTC2654::WRITE_AND_UPDATE_REG) {
			if (parameters.nargs() != 4) {
				console->write("Value is required to write, see help.\n");
				return;
			}

			// Not a double
			if (!parameters.parse_parameters(3, true, &arg)) {
				console->write("Invalid value parameter, see help.\n");
				return;
			}

			if (!toUint32(arg, val)) {
				console->write("Value is neither 0x (hex) or b (binary), see help.\n");
				return;
			}

			if (val > 0x0fff) {
				console->write("Value is higher then 2^12, see help.\n");
				return;
			}
		}

		dac.sendCommand(addr, cmd, val);
	}

private:
	LTC2654 &dac;
};

void LTC2654::registerConsoleCommands(CommandParser &parser, const std::string &prefix) {
	parser.register_command(prefix + "command", std::make_shared<LTC2654::Send>(*this));
}

void LTC2654::deregisterConsoleCommands(CommandParser &parser, const std::string &prefix) {
	parser.register_command(prefix + "command", NULL);
}
