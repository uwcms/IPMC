namespace {

class ConsoleCommand_flash_verify : public CommandParser::Command {
public:
	virtual std::string get_helptext(const std::string &command) const {
		return stdsprintf(
				"%s\n\n"
				"Check if the image in the QSPI flash is valid.\n", command.c_str());
	}

	virtual void execute(std::shared_ptr<ConsoleSvc> console, const CommandParser::CommandParameters &parameters) {
		std::unique_ptr<uint8_t> buf = std::unique_ptr<uint8_t>(new uint8_t[16 * 1024 * 1024]);

		if (!qspiflash->isInitialized()) qspiflash->initialize();

		qspiflash->read(0, &*buf, 16*1024*1024);

		BootFileValidationReturn r = validateBootFile(&*buf, 16*1024*1024);

		if (r == 0) {
			console->write("QSPI image is VALID\n", r);
		} else {
			console->write(stdsprintf("QSPI image is INVALID (reason: %d)\n", r));
		}
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
