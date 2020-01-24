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
		return command + " [$git_tag_prefix]\n"
				"\n"
				"This command configures the image tag lock feature, which prevents the "
				"IPMC from accepting update files that do not match the specified git tag "
				"prefix.  This can be used to prevent the accidental installation of images "
				"that may be electrically incompatible with the carrier board.  A separate "
				"mechanism also prevents the installation of an update built for a different "
				"IPMC ZYNQ part.\n"
				"\n"
				"This comparison ignores any semantic version number prefixed with \"-v\" at"
				"the end of the git tag, for example:\n"
				"  MyBoard-v1.2.3                       -> MyBoard\n"
				"  MyBoard-Gen2-v1.2.3                  -> MyBoard-Gen2\n"
				"  MyBoard-Gen2-v1.2.3-alpha4+20200131e -> MyBoard-Gen2\n"
				"\n"
				"If a firmware update does not match the git tag that has been specified with "
				"this setting, or begin with \"fallback-\", it will not be accepted.\n"
				"\n"
				"Without a value, this command will display the current setting.\n"
				"\n"
				"The special value \"?\" will lock to the next non-fallback image installed.\n"
				"The special value \"*\" will disable this protection completely.\n";
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
