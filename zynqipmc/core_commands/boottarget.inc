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

/// A "boottarget" command
class BootTargetCommand : public CommandParser::Command {
public:
	BootTargetCommand(Flash &flash, BootConfig &bootconf) : flash(flash), bootconf(bootconf) {};

	virtual std::string getHelpText(const std::string &command) const {
		return command + " [fallback|primary|backup|test]\n\n"
				"Retrieve or set the IPMC boot target.\n";
	}

	virtual void execute(std::shared_ptr<ConsoleSvc> console, const CommandParser::CommandParameters &parameters) {
		enum BootConfig::LogicalBootTarget lbt = this->bootconf.getLogicalBootTarget();

		if (parameters.nargs() == 1) {
			std::string msg = "Target boot image is ";
			switch (lbt) {
			case BootConfig::LBT_FALLBACK: msg += "fallback"; break;
			case BootConfig::LBT_PRIMARY: msg += "primary"; break;
			case BootConfig::LBT_BACKUP: msg += "backup"; break;
			case BootConfig::LBT_TEST: msg += "test"; break;
			default: msg += "not set"; break;
			}
			console->write(msg + ".\n");
		} else {
			BootConfig::LogicalBootTarget lbt;
			if (!parameters.parameters[1].compare("fallback")) {
				lbt = BootConfig::LBT_FALLBACK;
			} else if (!parameters.parameters[1].compare("primary")) {
				lbt = BootConfig::LBT_PRIMARY;
			} else if (!parameters.parameters[1].compare("backup")) {
				lbt = BootConfig::LBT_BACKUP;
			} else if (!parameters.parameters[1].compare("test")) {
				lbt = BootConfig::LBT_TEST;
			} else {
				console->write("Unknown image name, see help.\n");
				return;
			}

			if (flash.getTotalSize() == (16*1024*1024)) {
				console->write("This is a single partition device. The boot target cannot be configured.\n");
				console->write("The target boot image is now primary.\n");
				this->bootconf.setLogicalBootTarget(BootConfig::LBT_FALLBACK);
				this->bootconf.flushBootTarget();
				return;
			}
			else {
				// Stage 2. Verify the requested partition.
				std::unique_ptr<uint8_t> buf = std::unique_ptr<uint8_t>(new uint8_t[16*1024*1024]);

				this->flash.initialize();
				this->flash.read(bootconf.mapLogicalToPhysicalBootTarget(lbt) * 16*1024*1024, &*buf, 16*1024*1024);

				std::string message;
				std::shared_ptr<const VersionInfo> bin_version = NULL;
				BootFileValidationReturn r = validateBootFile(&*buf, 16*1024*1024, message, bin_version, &this->bootconf);
				if (r != 0) {
					console->write(stdsprintf("QSPI image INVALID: %s\n", message.c_str()));
					console->write("Unable to proceed.\n");
					return;
				}
				console->write(stdsprintf("QSPI image VALID: %s\n", message.c_str()));

				if (lbt == BootConfig::LBT_BACKUP) {
					// This is an update revert.
					enum BootConfig::PhysicalBootTarget wipe_pbt = this->bootconf.mapLogicalToPhysicalBootTarget(BootConfig::LBT_PRIMARY);
					this->bootconf.switchPrimaryImage();
					this->bootconf.setLogicalBootTarget(BootConfig::LBT_PRIMARY);
					this->bootconf.flushBootTarget();
					// Now that we have safely locked in our new primary image, we can invalidate the old one.
					const size_t flashSectorSize = this->flash.getSectorSize();
					uint8_t blankSector[flashSectorSize];
					memset(blankSector, 0xff, flashSectorSize);
					this->flash.write(wipe_pbt * 16*1024*1024, blankSector, flashSectorSize);
					console->write("The latest primary image update has been REVERTED.\n");
					console->write("The backup image is now the primary image.\n");
					console->write("The old primary image has been erased.\n");
				}
				else {
					// A normal update.
					this->bootconf.setLogicalBootTarget(lbt);
					this->bootconf.flushBootTarget();

				}
				std::string msg = "Target boot image is now ";
				switch (this->bootconf.getLogicalBootTarget()) {
				case BootConfig::LBT_FALLBACK: msg += "fallback"; break;
				case BootConfig::LBT_PRIMARY: msg += "primary"; break;
				case BootConfig::LBT_BACKUP: msg += "backup"; break;
				case BootConfig::LBT_TEST: msg += "test"; break;
				default: msg += "not set"; break;
				}
				console->write(msg + ".\n");
			}
		}
	}

private:
	Flash &flash;
	BootConfig &bootconf;
};

}
