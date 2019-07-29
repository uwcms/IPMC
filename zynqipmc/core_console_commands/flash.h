namespace {

class ConsoleCommand_flash_verify : public CommandParser::Command {
public:

	Flash &flash;

	ConsoleCommand_flash_verify(Flash &flash) : flash(flash) {};

	virtual std::string get_helptext(const std::string &command) const {
		return stdsprintf(
				"%s $partition\n\n"
				"Check if an image in the QSPI flash is valid.\n"
				"Allowed partitions are:\n"
				" IPMC revA: A\n"
				" IPMC revB: fallback, A, B, test\n", command.c_str());
	}

	virtual void execute(std::shared_ptr<ConsoleSvc> console, const CommandParser::CommandParameters &parameters) {
#define MB * (1024 * 1024)
		size_t address = 0;

		if (!flash.isInitialized()) flash.initialize();

		if (flash.getTotalSize() == (16 MB)) {
			if (!parameters.parameters[1].compare("A")) {
				address = 0;
			} else {
				console->write("IPMC revA only has image A, see help.\n");
				return;
			}
		} else {
			if (!parameters.parameters[1].compare("fallback")) {
				address = 0;
			} else if (!parameters.parameters[1].compare("A")) {
				address = 16 MB;
			} else if (!parameters.parameters[1].compare("B")) {
				address = 32 MB;
			} else if (!parameters.parameters[1].compare("test")) {
				address = 48 MB;
			} else {
				console->write("Unknown image name, see help.\n");
				return;
			}
		}

		std::unique_ptr<uint8_t> buf = std::unique_ptr<uint8_t>(new uint8_t[16 MB]);

		qspiflash->read(address, &*buf, 16 MB);

		BootFileValidationReturn r = validateBootFile(&*buf, 16 MB);

		if (r == 0) {
			console->write("QSPI image is VALID\n", r);
		} else {
			console->write(stdsprintf("QSPI image is INVALID (reason: %s)\n", getBootFileValidationErrorString(r)));
		}
#undef MB
	}
};

class ConsoleCommand_flash_info : public CommandParser::Command {
public:
	virtual std::string get_helptext(const std::string &command) const {
		return stdsprintf(
				"%s\n"
				"\n"
				"Info about the flash.\n", command.c_str());
	}

	virtual void execute(std::shared_ptr<ConsoleSvc> console, const CommandParser::CommandParameters &parameters) {
		if (!qspiflash->isInitialized()) qspiflash->initialize();

		console->write("Total flash size: " + bytesToString(qspiflash->getTotalSize()) + "\n");
	}
};

}
