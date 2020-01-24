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

#include "../libs/bootconfig/bootconfig.h"

namespace CoreCommands {

/// A "image_tag_lock" command
class ImageTagLockCommand : public CommandParser::Command {
public:
	ImageTagLockCommand(BootConfig &bootconf) : bootconf(bootconf) {};

	virtual std::string getHelpText(const std::string &command) const {
		return command + " [$image_tag]\n"
				"\n"
				"Lock firmware updates to only those from a specific tag/lineage.\n"
				"Without a value, it will display the current setting.\n"
				"\n"
				"All firmware updates must match either this git tag, or the \"fallback\" "
				"tag in order to be accepted.\n"
				"\n"
				"The special value \"?\" will lock to the next non-fallback image provided.\n"
				"The special value \"*\" will disable this protection.\n";
	}

	virtual void execute(std::shared_ptr<ConsoleSvc> console, const CommandParser::CommandParameters &parameters) {
		std::string old_value = this->bootconf.getImageTagLock();

		std::string tag;
		if (parameters.nargs() <= 1) {
			console->write(stdsprintf("The image tag lock is currently set to \"%s\".\n", old_value.c_str()));
			return;
		}

		std::string new_value;
		if (!parameters.parseParameters(1, true, &new_value)) {
			console->write("Incorrect parameters.\n");
			return;
		}
		this->bootconf.setImageTagLock(new_value);
		console->write(stdsprintf("Changed the image tag lock setting from \"%s\" to \"%s\".\n", old_value.c_str(), new_value.c_str()));
	}

private:
	BootConfig &bootconf;
};

}
