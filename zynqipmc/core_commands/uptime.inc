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

/// A "uptime" console command.
class UptimeCommand : public CommandParser::Command {
public:
	virtual std::string get_helptext(const std::string &command) const {
		return command + "\n\n"
				"Print the current system uptime.\n";
	}

	virtual void execute(std::shared_ptr<ConsoleSvc> console, const CommandParser::CommandParameters &parameters) {
		uint64_t now64 = get_tick64();
		//uint16_t ms = now64 % 1000;
		uint16_t s  = (now64 / 1000) % 60;
		uint16_t m  = (now64 / (60*1000)) % 60;
		uint16_t h  = (now64 / (60*60*1000)) % 24;
		uint32_t d  = (now64 / (24*60*60*1000));
		std::string out = "Up for ";
		if (d) out += stdsprintf("%lud", d);
		if (d||h) out += stdsprintf("%huh", h);
		if (d||h||m) out += stdsprintf("%hum", m);
		out += stdsprintf("%hus", s);
		console->write(out + "\n");
	}
};

}
