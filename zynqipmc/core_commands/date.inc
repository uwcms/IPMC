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

/// A "date" console command.
class DateCommand : public CommandParser::Command {
public:
	virtual std::string getHelpText(const std::string &command) const {
		return command + "\n\n"
				"Print the current system time. Updated by SNTP and kept by FreeRTOS.\n";
	}

	virtual void execute(std::shared_ptr<ConsoleSvc> console, const CommandParser::CommandParameters &parameters) {
		timeval tv;
		gettimeofday(&tv, nullptr);
		if (tv.tv_sec == 0) {
			console->write("Time information unavailable.\n");
		} else {
			struct tm *timeinfo = localtime(&tv.tv_sec);
			console->write(std::string(asctime(timeinfo))); // GMT
		}
	}
};

}
