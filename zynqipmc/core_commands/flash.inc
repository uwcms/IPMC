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

/// A "flash.verify" console command.
class FlashVerifyCommand : public CommandParser::Command {
public:
	FlashVerifyCommand(Flash &flash, BootConfig &bootconf) : flash(flash), bootconf(bootconf) {};

	virtual std::string getHelpText(const std::string &command) const {
		return stdsprintf(
				"%s [$partition]\n\n"
				"Check if an image in the QSPI flash is valid.\n"
				"Allowable partition values:\n"
				" IPMC revA: [value ignored]\n"
				" IPMC revB: fallback, primary, backup, test\n", command.c_str());
		// We are specifically leaving imageA and imageB undocumented.
	}

	virtual void execute(std::shared_ptr<ConsoleSvc> console, const CommandParser::CommandParameters &parameters) {
#define MB * (1024 * 1024)
		size_t address = 0;

		if (!flash.isInitialized()) flash.initialize();

		if (flash.getTotalSize() == (16 MB)) {
			address = 0;
		}
		else if (parameters.parameters.size() < 2) {
			console->write("Missing parameter. See help.\n");
			return;
		}
		else {
			if (!parameters.parameters[1].compare("fallback")) {
				address = 0;
			} else if (!parameters.parameters[1].compare("imageA")) {
				address = 16 MB;
			} else if (!parameters.parameters[1].compare("imageB")) {
				address = 32 MB;
			} else if (!parameters.parameters[1].compare("primary")) {
				address = this->bootconf.mapLogicalToPhysicalBootTarget(BootConfig::LBT_PRIMARY) * 16 MB;
			} else if (!parameters.parameters[1].compare("backup")) {
				address = this->bootconf.mapLogicalToPhysicalBootTarget(BootConfig::LBT_BACKUP) * 16 MB;
			} else if (!parameters.parameters[1].compare("test")) {
				address = 48 MB;
			} else {
				console->write("Unknown image name, see help.\n");
				return;
			}
		}

		std::unique_ptr<uint8_t> buf = std::unique_ptr<uint8_t>(new uint8_t[16 MB]);

		this->flash.read(address, &*buf, 16 MB);

		std::string message;
		std::shared_ptr<const VersionInfo> bin_version = NULL;
		BootFileValidationReturn r = validateBootFile(&*buf, 16 MB, message, bin_version, &this->bootconf);
		if (r != 0) {
			console->write(stdsprintf("QSPI image INVALID: %s\n", message.c_str()));
		} else {
			console->write(stdsprintf("QSPI image VALID: %s\n", message.c_str()));
		}

#undef MB
	}

private:
	Flash &flash;
	BootConfig &bootconf;
};

/// A "flash.info" console command.
class FlashInfoCommand : public CommandParser::Command {
public:
	FlashInfoCommand(Flash &flash) : flash(flash) {};

	virtual std::string getHelpText(const std::string &command) const {
		return command + "\n\n"
				"Info about the flash.\n";
	}

	virtual void execute(std::shared_ptr<ConsoleSvc> console, const CommandParser::CommandParameters &parameters) {
		if (!qspiflash->isInitialized()) qspiflash->initialize();

		console->write("Total flash size: " + bytesToString(qspiflash->getTotalSize()) + "\n");
	}

private:
	Flash &flash;
};

}
