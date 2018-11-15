namespace {

class ConsoleCommand_flash_info : public CommandParser::Command {
public:
	virtual std::string get_helptext(const std::string &command) const {
		return stdsprintf(
				"%s\n"
				"\n"
				"info about the flash.\n", command.c_str());
	}

	virtual void execute(std::shared_ptr<ConsoleSvc> console, const CommandParser::CommandParameters &parameters) {
		std::string info = "Flash is a " +
				isfqspi->GetManufacturerName() +
				" IC with a total of " +
				std::to_string(isfqspi->GetTotalSize() / 1024 / 1024) +
				"MBytes.\n";

		info += "Sector size: " + std::to_string(isfqspi->GetSectorSize()) + "\n";
		info += "Page size: " + std::to_string(isfqspi->GetPageSize()) + "\n";

		console->write(info);
	}

	//virtual std::vector<std::string> complete(const CommandParser::CommandParameters &parameters) const { };
};

}
