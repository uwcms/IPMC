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

/// A "reboot_status" console command.
class RebootStatusCommand : public CommandParser::Command {
public:
	virtual std::string getHelpText(const std::string &command) const {
		return command + "\n\n"
				"Print and parse the value of the REBOOT_STATUS register.\n";
	}

	virtual void execute(std::shared_ptr<ConsoleSvc> console, const CommandParser::CommandParameters &parameters) {
		std::string out = "";
		out += stdsprintf("The value of the REBOOT_STATUS register on startup was 0x%08lx.\n", reboot_status);
		out += stdsprintf("The user \"REBOOT_STATE\" field is 0x%02lx.\n", reboot_status >> 24);
		out += stdsprintf("The reset reason field is 0x%02lx.\n", (reboot_status >> 16) & 0xff);
		const std::vector< std::pair<int, std::string> > reasons({
			{22, "  POR:       Power On Reset"},
			{21, "  SRST_B:    PS_SRST_B reset signal"},
			{20, "  DBG_RST:   Debug command in the DAP controller"},
			{19, "  SLC_RST:   Software initiated (used by fallback bootloader to select image)"},
			{18, "  AWDT1_RST: CPU 1 watchdog timer"},
			{17, "  AWDT0_RST: CPU 0 watchdog timer"},
			{16, "  SWDT_RST:  System watchdog timer"},
		});
		for (auto reason : reasons)
			if (reboot_status & (1<<reason.first))
				out += reason.second + "\n";
		out += stdsprintf("The BootROM error code was 0x%04lx.\n", reboot_status & 0x0000ffff);
		console->write(out);
	}
};

}
