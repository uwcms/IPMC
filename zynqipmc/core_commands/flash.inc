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

#define MB *(1024 * 1024)
	virtual void execute(std::shared_ptr<ConsoleSvc> console, const CommandParser::CommandParameters &parameters) {
		if (!flash.isInitialized()) flash.initialize();

		if (flash.getTotalSize() == (16 MB)) {
			this->execute_one(console, BootConfig::PBT_FALLBACK);
			return;
		}
		else if (parameters.parameters.size() < 2) {
			// Do all of them.
			this->execute_one(console, BootConfig::PBT_FALLBACK);
			this->execute_one(console, this->bootconf.mapLogicalToPhysicalBootTarget(BootConfig::LBT_PRIMARY));
			this->execute_one(console, this->bootconf.mapLogicalToPhysicalBootTarget(BootConfig::LBT_BACKUP));
			this->execute_one(console, BootConfig::PBT_TEST);
			return;
		}
		else {
			if (!parameters.parameters[1].compare("fallback")) {
				this->execute_one(console, BootConfig::PBT_FALLBACK);
			}
			else if (!parameters.parameters[1].compare("imageA")) {
				this->execute_one(console, BootConfig::PBT_A);
			}
			else if (!parameters.parameters[1].compare("imageB")) {
				this->execute_one(console, BootConfig::PBT_B);
			}
			else if (!parameters.parameters[1].compare("primary")) {
				this->execute_one(console, this->bootconf.mapLogicalToPhysicalBootTarget(BootConfig::LBT_PRIMARY));
			}
			else if (!parameters.parameters[1].compare("backup")) {
				this->execute_one(console, this->bootconf.mapLogicalToPhysicalBootTarget(BootConfig::LBT_BACKUP));
			}
			else if (!parameters.parameters[1].compare("test")) {
				this->execute_one(console, BootConfig::PBT_TEST);
			}
			else {
				console->write("Unknown image name, see help.\n");
				return;
			}
		}
	}

	virtual void execute_one(std::shared_ptr<ConsoleSvc> console, BootConfig::PhysicalBootTarget chosen_partition, std::string prefix = "") {
		std::unique_ptr<uint8_t> buf = std::unique_ptr<uint8_t>(new uint8_t[16 MB]);
		BootConfig::PhysicalBootTarget physicalbt = this->bootconf.getPhysicalBootTarget();
		BootConfig::PhysicalBootTarget primarybt = this->bootconf.mapLogicalToPhysicalBootTarget(BootConfig::LBT_PRIMARY);
		std::string part_desc, part_status, part_active;
		if (flash.getTotalSize() == (16 MB)) {
			part_desc = "Single-image flash chip";
		}
		else {
			switch (chosen_partition) {
			case BootConfig::PBT_FALLBACK:
				part_desc = "Fallback Partition";
				break;
			case BootConfig::PBT_A:
				part_desc = "Partition A";
				part_status = (primarybt == BootConfig::PBT_A) ? "Primary" : "Backup";
				break;
			case BootConfig::PBT_B:
				part_desc = "Partition B";
				part_status = (primarybt == BootConfig::PBT_B) ? "Primary" : "Backup";
				break;
			case BootConfig::PBT_TEST:
				part_desc = "Test Partition";
				break;
			default:
				part_desc = "Unknown Partition";
			}
			if (chosen_partition == physicalbt)
				part_active = "Active Boot Target";
		}
		std::string label = part_desc;
		if (part_status.size())
			label += std::string("; ") + part_status;
		if (part_active.size())
			label += std::string("; ") + part_active;

		this->flash.read(chosen_partition * 16 MB, &*buf, 16 MB);

		std::string message;
		std::shared_ptr<const VersionInfo> bin_version = NULL;
		BootFileValidationReturn r = validateBootFile(&*buf, 16 MB, message, bin_version, &this->bootconf);

		if (r != BFV_VALID) {
			console->write(stdsprintf("[%s]\nQSPI image INVALID: %s\n", label.c_str(), message.c_str()));
		}
		else {
			console->write(stdsprintf("[%s]\nQSPI image VALID: %s\n", label.c_str(), message.c_str()));
		}
	}
#undef MB

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
